import UIKit
import WebKit
import MediaPlayer

final class ViewController: UIViewController {

    private static let iCloudPresetsKey = "norcoast_presets"

    private var webView: WKWebView!

    // MARK: - Lifecycle

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 3/255, green: 5/255, blue: 8/255, alpha: 1)
        setupWebView()
        setupNowPlaying()
        setupRemoteControls()
        setupiCloudSync()
        loadApp()
    }

    // MARK: - WebView

    private func setupWebView() {
        let config = WKWebViewConfiguration()

        // Allow inline audio and remove the extra user-gesture requirement.
        // The web app already has a "Tap to begin" gate that satisfies the gesture.
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []

        // Signal to the web app that it is running inside the native wrapper so it
        // can skip redundant native-workaround code (e.g. the MediaStream silent-mode hack).
        let flag = WKUserScript(
            source: "window.__NATIVE_APP__ = true;",
            injectionTime: .atDocumentStart,
            forMainFrameOnly: true
        )
        let ucc = WKUserContentController()
        ucc.addUserScript(flag)
        ucc.add(self, name: "bridge")
        config.userContentController = ucc

        webView = WKWebView(frame: .zero, configuration: config)
        webView.translatesAutoresizingMaskIntoConstraints = false
        webView.scrollView.isScrollEnabled = false
        webView.scrollView.bounces = false
        webView.scrollView.contentInsetAdjustmentBehavior = .never
        webView.isOpaque = false
        webView.backgroundColor = view.backgroundColor
        webView.scrollView.backgroundColor = view.backgroundColor
        webView.navigationDelegate = self

        view.addSubview(webView)
        NSLayoutConstraint.activate([
            webView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            webView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            webView.topAnchor.constraint(equalTo: view.topAnchor),
            webView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])
    }

    private func loadApp() {
        // public/ is bundled as a folder reference named "public" inside the app bundle.
        guard let resourceURL = Bundle.main.resourceURL else { return }
        let webRoot = resourceURL.appendingPathComponent("public")
        let indexURL = webRoot.appendingPathComponent("index.html")
        webView.loadFileURL(indexURL, allowingReadAccessTo: webRoot)
    }

    // MARK: - iCloud KV Sync

    private func setupiCloudSync() {
        let kv = NSUbiquitousKeyValueStore.default
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(iCloudDidChange),
            name: NSUbiquitousKeyValueStore.didChangeExternallyNotification,
            object: kv
        )
        kv.synchronize()
        // Injection deferred to webView(_:didFinish:) — page must load first.
    }

    @objc private func iCloudDidChange(_ notification: Notification) {
        injectCloudPresets()
    }

    private func injectCloudPresets() {
        guard let json = NSUbiquitousKeyValueStore.default.string(forKey: Self.iCloudPresetsKey) else { return }
        // json is already valid JSON — embed directly as a JS object literal, no escaping needed.
        webView.evaluateJavaScript("norcoast_setCloudPresets(\(json))", completionHandler: nil)
    }

    private func savePresetsToiCloud(_ json: String) {
        NSUbiquitousKeyValueStore.default.set(json, forKey: Self.iCloudPresetsKey)
        NSUbiquitousKeyValueStore.default.synchronize()
    }

    // MARK: - Lock Screen / Now Playing

    private func setupNowPlaying() {
        var info: [String: Any] = [
            MPMediaItemPropertyTitle: "Norcoast Ambience",
            MPMediaItemPropertyArtist: "Norcoast Audio",
            MPNowPlayingInfoPropertyIsLiveStream: true,
        ]
        if let icon = UIImage(named: "AppIcon") {
            info[MPMediaItemPropertyArtwork] = MPMediaItemArtwork(boundsSize: icon.size) { _ in icon }
        }
        MPNowPlayingInfoCenter.default().nowPlayingInfo = info
        MPNowPlayingInfoCenter.default().playbackState = .playing
    }

    private func setupRemoteControls() {
        UIApplication.shared.beginReceivingRemoteControlEvents()
        let cc = MPRemoteCommandCenter.shared()

        cc.playCommand.isEnabled = true
        cc.pauseCommand.isEnabled = true
        cc.togglePlayPauseCommand.isEnabled = true
        cc.stopCommand.isEnabled = false
        cc.nextTrackCommand.isEnabled = false
        cc.previousTrackCommand.isEnabled = false

        cc.playCommand.addTarget { [weak self] _ in
            self?.setPlayback(true)
            return .success
        }
        cc.pauseCommand.addTarget { [weak self] _ in
            self?.setPlayback(false)
            return .success
        }
        cc.togglePlayPauseCommand.addTarget { [weak self] _ in
            self?.sendToWebApp("norcoast_togglePause()")
            return .success
        }
    }

    // MARK: - JS Bridge

    private func sendToWebApp(_ js: String) {
        webView.evaluateJavaScript(js, completionHandler: nil)
    }

    private func setPlayback(_ playing: Bool) {
        sendToWebApp(playing ? "norcoast_resume()" : "norcoast_pause()")
        MPNowPlayingInfoCenter.default().playbackState = playing ? .playing : .paused
    }

    // MARK: - System UI

    override var prefersStatusBarHidden: Bool { true }
    override var preferredStatusBarUpdateAnimation: UIStatusBarAnimation { .fade }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask {
        // iPhone stays portrait-only; iPad rotates freely.
        UIDevice.current.userInterfaceIdiom == .pad ? .all : .portrait
    }
}

// MARK: - NorcoastAudioControllable

extension ViewController: NorcoastAudioControllable {
    func pauseForInterruption() { setPlayback(false) }
    func resumeAfterInterruption() { setPlayback(true) }
}

// MARK: - WKNavigationDelegate

extension ViewController: WKNavigationDelegate {
    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        decisionHandler: @escaping (WKNavigationActionPolicy) -> Void
    ) {
        decisionHandler(navigationAction.navigationType == .linkActivated ? .cancel : .allow)
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        // Page is fully loaded — safe to inject iCloud presets into JS globals.
        injectCloudPresets()
    }
}

// MARK: - WKScriptMessageHandler

extension ViewController: WKScriptMessageHandler {
    func userContentController(
        _ userContentController: WKUserContentController,
        didReceive message: WKScriptMessage
    ) {
        guard message.name == "bridge", let body = message.body as? String else { return }
        if body.hasPrefix("presets:") {
            savePresetsToiCloud(String(body.dropFirst("presets:".count)))
            return
        }
        switch body {
        case "playing":
            MPNowPlayingInfoCenter.default().playbackState = .playing
        case "paused":
            MPNowPlayingInfoCenter.default().playbackState = .paused
        case "haptic":
            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
        default:
            break
        }
    }
}
