import UIKit
import WebKit
import MediaPlayer

final class ViewController: UIViewController {

    private var webView: WKWebView!

    // MARK: - Lifecycle

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = UIColor(red: 3/255, green: 5/255, blue: 8/255, alpha: 1)
        setupWebView()
        setupNowPlaying()
        setupRemoteControls()
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
            self?.sendToWebApp("norcoast_resume()")
            MPNowPlayingInfoCenter.default().playbackState = .playing
            return .success
        }
        cc.pauseCommand.addTarget { [weak self] _ in
            self?.sendToWebApp("norcoast_pause()")
            MPNowPlayingInfoCenter.default().playbackState = .paused
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

    /// Called by SceneDelegate after a phone-call / Siri interruption ends.
    func resumeAfterInterruption() {
        sendToWebApp("norcoast_resume()")
        MPNowPlayingInfoCenter.default().playbackState = .playing
    }

    // MARK: - System UI

    override var prefersStatusBarHidden: Bool { true }
    override var preferredStatusBarUpdateAnimation: UIStatusBarAnimation { .fade }
    override var prefersHomeIndicatorAutoHidden: Bool { true }
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .portrait }
}

// MARK: - WKNavigationDelegate

extension ViewController: WKNavigationDelegate {
    func webView(
        _ webView: WKWebView,
        decidePolicyFor navigationAction: WKNavigationAction,
        decisionHandler: @escaping (WKNavigationActionPolicy) -> Void
    ) {
        // Block any external links — all navigation stays local.
        if navigationAction.navigationType == .linkActivated {
            decisionHandler(.cancel)
        } else {
            decisionHandler(.allow)
        }
    }
}

// MARK: - WKScriptMessageHandler

extension ViewController: WKScriptMessageHandler {
    func userContentController(
        _ userContentController: WKUserContentController,
        didReceive message: WKScriptMessage
    ) {
        guard message.name == "bridge", let body = message.body as? String else { return }
        switch body {
        case "playing":
            MPNowPlayingInfoCenter.default().playbackState = .playing
        case "paused":
            MPNowPlayingInfoCenter.default().playbackState = .paused
        default:
            break
        }
    }
}
