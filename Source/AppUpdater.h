/*
  ==============================================================================

    AppUpdater.h
    Created: 8 Apr 2017 4:26:46pm
    Author:  Ben

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "QueuedNotifier.h"

class AppUpdateEvent
{
public:
	enum Type
	{
		UPDATE_AVAILABLE,
		DOWNLOAD_STARTED,
		DOWNLOAD_PROGRESS,
		DOWNLOAD_ERROR,
		UPDATE_FINISHED
	};

	AppUpdateEvent(Type t, File f = File()) :
		type(t),
		file(f)
	{
	}
	AppUpdateEvent(Type t, String title, String msg, String changelog) :
		type(t),
		title(title),
		msg(msg),
		changelog(changelog)
	{
	}
	AppUpdateEvent(Type t, float progression) :
		type(t),
		progression(progression)
	{
	}
	Type type;
	File file;

	String title;
	String msg;
	String changelog;

	float progression;
};

typedef QueuedNotifier<AppUpdateEvent>::Listener AppUpdaterAsyncListener;


class UpdateDialogWindow : public Component,
						   public Button::Listener,
						   public AppUpdaterAsyncListener
{
public:
	UpdateDialogWindow(const String &msg, const String &changelog);
	~UpdateDialogWindow();
	Label msgLabel;
	TextEditor changelogLabel;
	Label statusLabel;

	TextButton okButton;
	TextButton cancelButton;
	TextButton manualDownload;

	Rectangle<int> progressionRect;

	bool errorMode;

	void paint(Graphics &g) override;
	void resized() override;

	void newMessage(const AppUpdateEvent &e) override;

	void buttonClicked(Button *b) override;
};



class AppUpdater : public Thread,
				   public URL::DownloadTask::Listener,
				   public AppUpdaterAsyncListener
{
public:
	juce_DeclareSingleton(AppUpdater, true);

	AppUpdater();
	~AppUpdater();

	URL updateURL;
	String downloadURLBase;
	String downloadingFileName;
	String filePrefix;
	File targetDir;
	File targetFile;
	bool isInstaller;
	String fileExtension;

	std::unique_ptr<UpdateDialogWindow> updateWindow;

	float progression;

	std::unique_ptr<URL::DownloadTask> downloadTask;

	void setURLs(URL _updateURL, String _downloadURLBase, String filePrefix);

	String getDownloadFileName(String version, String extension);
	void checkForUpdates();

	void showDialog(String title, String msg, String changelog);
	void downloadUpdate();


	bool versionIsNewerThan(String versionToCheck, String referenceVersion);

	// Inherited via Thread
	virtual void run() override;

	// Inherited via Listener
	virtual void finished(URL::DownloadTask *task, bool success) override;
	virtual void progress(URL::DownloadTask *task, int64 bytesDownloaded, int64 totalLength) override;

	virtual void newMessage(const AppUpdateEvent &e) override;

	QueuedNotifier<AppUpdateEvent> queuedNotifier;
	void addAsyncUpdateListener(AppUpdaterAsyncListener *newListener) { queuedNotifier.addListener(newListener); }
	void addAsyncCoalescedUpdateListener(AppUpdaterAsyncListener *newListener) { queuedNotifier.addAsyncCoalescedListener(newListener); }
	void removeAsyncUpdateListener(AppUpdaterAsyncListener *listener) { queuedNotifier.removeListener(listener); }
};
