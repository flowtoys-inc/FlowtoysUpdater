#include "AppUpdater.h"
#include "VersionUtils.h"
/*
  ==============================================================================

	AppUpdater.cpp
	Created: 8 Apr 2017 4:26:46pm
	Author:  Ben

  ==============================================================================
*/


juce_ImplementSingleton(AppUpdater)

#if JUCE_MAC //for chmod
#include <sys/types.h>
#include <sys/stat.h>
#endif


AppUpdater::AppUpdater() :
	Thread("appUpdater"),
	progression(0),
	queuedNotifier(30)
{
	addAsyncUpdateListener(this);
}

AppUpdater::~AppUpdater()
{
	queuedNotifier.cancelPendingUpdate();
	signalThreadShouldExit();
	waitForThreadToExit(1000);
}

void AppUpdater::setURLs(URL _updateURL, String _downloadURLBase, String _filePrefix)
{
	updateURL = _updateURL;
	filePrefix = _filePrefix;
	downloadURLBase = _downloadURLBase;
	if (!downloadURLBase.endsWithChar('/')) downloadURLBase += "/";
}

String AppUpdater::getDownloadFileName(String version, String extension)
{
	return VersionUtils::downloadFileName(filePrefix, VersionUtils::platformOSSuffix(), version, extension);
}

void AppUpdater::checkForUpdates()
{
	if (updateURL.isEmpty() || downloadURLBase.isEmpty()) return;
	startThread();
}

void AppUpdater::showDialog(String title, String msg, String changelog)
{

	progression = 0;

	updateWindow.reset(new UpdateDialogWindow(msg, changelog));
	DialogWindow::LaunchOptions dw;
	dw.content.set(updateWindow.get(), false);
	dw.dialogTitle = title;
	dw.useNativeTitleBar = false;
	dw.escapeKeyTriggersCloseButton = true;
	dw.dialogBackgroundColour = Colour(0x555555);
	dw.launchAsync();

}

void AppUpdater::downloadUpdate()
{

	DBG("Download file name " << downloadingFileName);

	targetDir.createDirectory();

	if (isInstaller)
	{
		targetFile = File::getSpecialLocation(File::tempDirectory).getChildFile(downloadingFileName);
	}else
	{
		targetFile = targetDir.getChildFile(downloadingFileName);
		if (targetFile.existsAsFile()) targetFile.deleteFile();
	}

	downloadingFileName = targetFile.getFileName();

	URL downloadURL = URL(downloadURLBase + URL::addEscapeChars(downloadingFileName, false));

	DBG("Downloading " + downloadURL.toString(false) + "...");
	downloadTask = downloadURL.downloadToFile(targetFile, URL::DownloadTaskOptions().withListener(this));

	if (downloadTask == nullptr)
	{
		DBG("Error while downloading " + downloadingFileName + ",\ntry downloading it directly from the website.");
		queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_ERROR));
	}
	queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_STARTED));
}

void AppUpdater::run()
{
	//First cleanup update_temp directory
	targetDir = File::getSpecialLocation(File::currentApplicationFile).getParentDirectory().getChildFile("update_temp");
	if (targetDir.exists()) targetDir.deleteRecursively();


	StringPairArray responseHeaders;
	int statusCode = 0;
	std::unique_ptr<InputStream> stream(updateURL.createInputStream(URL::InputStreamOptions(URL::ParameterHandling::inAddress)
		.withConnectionTimeoutMs(2000)
		.withResponseHeaders(&responseHeaders)
		.withStatusCode(&statusCode)));

	if (statusCode != 200)
	{
		DBG("Failed to connect, status code = " + String(statusCode));
		return;
	}

	DBG("AppUpdater:: Status code " << statusCode);

	if (stream != nullptr)
	{
		String content = stream->readEntireStreamAsString();
		var data = JSON::parse(content);

		if (data.isObject())
		{

#if !JUCE_DEBUG
			if (data.getProperty("testing", false)) return;
#endif


			DBG(JSON::toString(data));

			String version = data.getProperty("version", "");

			if (versionIsNewerThan(version, ProjectInfo::versionString))
			{
				String msg = "A new version of " + String(ProjectInfo::projectName) + " is available : " + version + ", do you want to update the app ?\nYou can also deactivate updates in the preferences.\n \
If the auto-update fails, you can always download and replace it manually.";

				Array<var> * changelog = data.getProperty("changelog", var()).getArray();
				String changelogString = "Changes since your version :\n\n";
				changelogString += "Version " + version + ":\n";
				if (changelog != nullptr) for (auto &c : *changelog) changelogString += c.toString() + "\n";
				changelogString += "\n\n";

				Array<var> * oldChangelogs = data.getProperty("archives", var()).getArray();
				if (oldChangelogs != nullptr) for (int i = oldChangelogs->size() - 1; i >= 0; i--)
				{
					var ch = oldChangelogs->getUnchecked(i);
					String chVersion = ch.getProperty("version", "1.0.0");
					if (versionIsNewerThan(ProjectInfo::versionString, chVersion)) break;

					changelogString += "Version " + chVersion + ":\n";
					Array<var> * versionChangelog = ch.getProperty("changelog", var()).getArray();
					if (versionChangelog != nullptr) for (auto &c : *versionChangelog) changelogString += c.toString() + "\n";
					changelogString += "\n\n";
				}


				String title = "New version available";

				fileExtension = "zip";

				String osToCheck = "";
#if JUCE_WINDOWS
				osToCheck = "win";
#elif JUCE_MAC
				osToCheck = "osx";
#elif JUCE_LINUX
				osToCheck = "linux";
#endif

				String installerExtension = data.getProperty(osToCheck + "Installer", "");
				isInstaller = installerExtension.isNotEmpty();
				fileExtension = isInstaller ? installerExtension : data.getProperty(osToCheck + "Extension", "zip").toString();
                
				downloadingFileName = getDownloadFileName(version, fileExtension);

				queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::UPDATE_AVAILABLE, title, msg, changelogString));

			}
			else
			{
				DBG("App is up to date :) (Latest version online : " << version << ")");
			}
		}
		else
		{
			DBG("Error while checking updates, update file is not valid");
		}

	}
	else
	{
		DBG("Error while trying to access to the update file");
	}
}

void AppUpdater::finished(URL::DownloadTask * task, bool success)
{
	if (!success)
	{
		DBG("Error while downloading " + downloadingFileName + ",\ntry downloading it directly from the website.\nError code : " + String(task->statusCode()));
		queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_ERROR));
		return;
	}

	File f = targetFile;
	File appFile = File::getSpecialLocation(File::currentApplicationFile);
	File appDir = appFile.getParentDirectory();

	if (!f.exists())
	{
		DBG("File doesn't exist");
		queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_ERROR));
		return;
	}

	if (f.getSize() < 1000000) //if file is less than 1Mo, got a problem
	{
		DBG("Wrong file size, try downloading it directly from the website");
		queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_ERROR));
		return;
	}

	if (isInstaller)
	{
		appFile = File::getSpecialLocation(File::tempDirectory).getChildFile(ProjectInfo::projectName + String("_install." + fileExtension));
		f.copyFileTo(appFile);
	}else
	{
		File td = f.getParentDirectory();
		ZipFile zip(f);
		zip.uncompressTo(td);
		Array<File> filesToCopy;

		appFile.moveFileTo(td.getNonexistentChildFile("oldApp", appFile.getFileExtension()));


		DBG("Move to " << appDir.getFullPathName());
		for (int i = 0; i < zip.getNumEntries(); i++)
		{
			File zf = td.getChildFile(zip.getEntry(i)->filename);
			DBG("File exists : " << (int)f.exists());
			zf.copyFileTo(appDir.getChildFile(zip.getEntry(i)->filename));
			//DBG("Move result for " << zf.getFileName() << " = " << (int)result);
		}

		File curAppFile = File::getSpecialLocation(File::currentApplicationFile);
		File curAppDir = curAppFile.getParentDirectory();
		File tempDir = curAppDir.getChildFile("update_temp");
		tempDir.deleteRecursively();

#if JUCE_MAC
		appFile = File::getSpecialLocation(File::currentApplicationFile).getParentDirectory().getChildFile(ProjectInfo::projectName + String(".app"));
		File af = appFile.getChildFile("Contents")
			.getChildFile("MacOS")
			.getChildFile(ProjectInfo::projectName);

		af.setExecutePermission(true);
		chmod(af.getFullPathName().toUTF8(), S_IRWXO | S_IRWXU | S_IRWXG);
		DBG(af.getFullPathName());
#endif
	}

	queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::UPDATE_FINISHED, f));

	MessageManagerLock mmLock;

	if (mmLock.lockWasGained())
	{

		DBG("Here");
		if (updateWindow != nullptr) updateWindow->removeFromDesktop();
        
        DBG("Start process : " << appFile.getFullPathName());
		bool result = appFile.startAsProcess();
        
        DBG("Process result : " << (int)result);
        if(!isInstaller || !result) appFile.revealToUser();
        
        DBG("Quit");
        JUCEApplication::quit();
        
	}


}

void AppUpdater::progress(URL::DownloadTask *, int64 bytesDownloaded, int64 totalLength)
{
    //No Content-Length header: fall back to a rough size estimate on every
    //platform (this was mac-only, leaving win/linux with totalLength = -1
    //and a negative progression, #21).
    if(totalLength <= 0) totalLength = 10000000; //around 10Mo to expect for app
    progression = jlimit(0.0f, 1.0f, (float)bytesDownloaded / (float)totalLength);
    DBG("Progress  " << progression << " (" << bytesDownloaded << " / " << totalLength << ")");
    queuedNotifier.addMessage(new AppUpdateEvent(AppUpdateEvent::DOWNLOAD_PROGRESS, progression));
}

void AppUpdater::newMessage(const AppUpdateEvent & e)
{
	switch (e.type)
	{
	case AppUpdateEvent::UPDATE_AVAILABLE:
		showDialog(e.title, e.msg, e.changelog);
		break;

		//case AppUpdateEvent::DOWNLOAD_ERROR:
	case AppUpdateEvent::UPDATE_FINISHED:
		updateWindow->getTopLevelComponent()->exitModalState(0);
		break;

	default:
		break;
	}
}

UpdateDialogWindow::UpdateDialogWindow(const String & msg, const String & changelog) :
	okButton("Update"),
	cancelButton("Cancel"),
	manualDownload("Download Manually"),
	errorMode(false)
{
	addAndMakeVisible(&msgLabel);
	addAndMakeVisible(&changelogLabel);
	addAndMakeVisible(&statusLabel);

	msgLabel.setColour(msgLabel.textColourId, Colours::lightgrey);
	msgLabel.setText(msg, dontSendNotification);
	statusLabel.setColour(statusLabel.textColourId, Colours::lightgrey);
	statusLabel.setText("Click \"Update\" to continue...", dontSendNotification);

	changelogLabel.setMultiLine(true);
	changelogLabel.setColour(changelogLabel.backgroundColourId, Colours::darkgrey.darker());
	changelogLabel.setColour(changelogLabel.textColourId, Colours::lightgrey);
	changelogLabel.setScrollBarThickness(8);
	changelogLabel.setReadOnly(true);
	changelogLabel.setText(changelog);

	addAndMakeVisible(&okButton);
	addAndMakeVisible(&cancelButton);
	addAndMakeVisible(&manualDownload);

	okButton.addListener(this);
	cancelButton.addListener(this);
	manualDownload.addListener(this);

	setSize(500, 400);

	AppUpdater::getInstance()->addAsyncUpdateListener(this);
}

UpdateDialogWindow::~UpdateDialogWindow()
{
	if (AppUpdater::getInstanceWithoutCreating() != nullptr) AppUpdater::getInstance()->removeAsyncUpdateListener(this);
}

void UpdateDialogWindow::paint(Graphics & g)
{
	g.fillAll(Colours::darkgrey);

	g.setColour(Colour(0xff111111));
	g.fillRoundedRectangle(progressionRect.toFloat(), 2);
	g.setColour(errorMode ? Colours::orangered : Colours::limegreen);
	g.fillRoundedRectangle(progressionRect.toFloat().withWidth(progressionRect.getWidth()*(errorMode ? 1 : AppUpdater::getInstance()->progression)), 2);
}

void UpdateDialogWindow::resized()
{
	Rectangle<int> r = getLocalBounds().reduced(10);
	Rectangle<int> br = r.removeFromBottom(20);
	cancelButton.setBounds(br.removeFromRight(100));
	br.removeFromRight(10);
	okButton.setBounds(br.removeFromRight(100));
	br.removeFromRight(10);
	manualDownload.setBounds(br.removeFromRight(150));

	r.removeFromBottom(10);

	progressionRect = r.removeFromBottom(8);
	r.removeFromBottom(2);

	statusLabel.setBounds(r.removeFromBottom(16));
	r.removeFromBottom(30);

	msgLabel.setBounds(r.removeFromTop(100));
	r.removeFromTop(10);
	changelogLabel.setBounds(r);

}

void UpdateDialogWindow::newMessage(const AppUpdateEvent & e)
{
	switch (e.type)
	{
	case AppUpdateEvent::DOWNLOAD_STARTED:
		statusLabel.setText("Downloading update...", dontSendNotification);
		break;

	case AppUpdateEvent::DOWNLOAD_PROGRESS:
		repaint();
		break;
	case AppUpdateEvent::DOWNLOAD_ERROR:
	{
		statusLabel.setText("There was a problem with the auto-update, please download the file manually.", dontSendNotification);
		statusLabel.setColour(statusLabel.textColourId, Colours::orangered);
		errorMode = true;
		repaint();
		okButton.setEnabled(false);
	}
	break;
	}
}

void UpdateDialogWindow::buttonClicked(Button * b)
{
	if (b == &okButton)
	{
		AppUpdater::getInstance()->downloadUpdate();
	}
	else if (b == &cancelButton)
	{
		getTopLevelComponent()->exitModalState(0);
	}
	else if (b == &manualDownload)
	{
		URL downloadURL = URL(AppUpdater::getInstance()->downloadURLBase + AppUpdater::getInstance()->downloadingFileName);
		downloadURL.launchInDefaultBrowser();
		getTopLevelComponent()->exitModalState(0);
	}
}


bool AppUpdater::versionIsNewerThan(String versionToCheck, String referenceVersion)
{
	return VersionUtils::versionIsNewerThan(versionToCheck, referenceVersion);
}
