import UIKit
import AVFoundation
import Sentry

class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    func scene(
        _ scene: UIScene,
        willConnectTo session: UISceneSession,
        options connectionOptions: UIScene.ConnectionOptions
    ) {
        guard let windowScene = scene as? UIWindowScene else { return }

        configureSentry()
        configureAudioSession()

        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = ViewController()
        window.makeKeyAndVisible()
        self.window = window
    }

    // MARK: - Crash Reporting

    private func configureSentry() {
        // Replace YOUR_SENTRY_DSN with the DSN from sentry.io → Project Settings → Client Keys.
        let dsn = "YOUR_SENTRY_DSN"
        guard dsn != "YOUR_SENTRY_DSN" else { return }
        SentrySDK.start { options in
            options.dsn = dsn
            options.tracesSampleRate = 0.2
        }
    }

    // MARK: - Audio Session

    private func configureAudioSession() {
        do {
            // .playback: overrides the silent switch and enables true background audio.
            // The web app's MediaStreamDestination hack is redundant here but harmless.
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
        } catch {
            print("AVAudioSession setup failed: \(error)")
        }

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleAudioInterruption),
            name: AVAudioSession.interruptionNotification,
            object: nil
        )
    }

    // Re-activate after phone calls, Siri, etc.
    @objc private func handleAudioInterruption(_ notification: Notification) {
        guard
            let info = notification.userInfo,
            let typeValue = info[AVAudioSessionInterruptionTypeKey] as? UInt,
            let type = AVAudioSession.InterruptionType(rawValue: typeValue)
        else { return }

        switch type {
        case .began:
            if let vc = window?.rootViewController as? ViewController {
                vc.pauseForInterruption()
            }
        case .ended:
            try? AVAudioSession.sharedInstance().setActive(true)
            let optionsValue = info[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0
            let options = AVAudioSession.InterruptionOptions(rawValue: optionsValue)
            if options.contains(.shouldResume) {
                // Short delay lets the audio system stabilise before we resume.
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                    if let vc = self.window?.rootViewController as? ViewController {
                        vc.resumeAfterInterruption()
                    }
                }
            }
        @unknown default:
            break
        }
    }
}
