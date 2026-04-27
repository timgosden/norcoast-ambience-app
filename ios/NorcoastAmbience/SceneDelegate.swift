import UIKit
import AVFoundation

protocol NorcoastAudioControllable: AnyObject {
    func pauseForInterruption()
    func resumeAfterInterruption()
}

class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    // MARK: - Lifecycle

    func scene(
        _ scene: UIScene,
        willConnectTo session: UISceneSession,
        options connectionOptions: UIScene.ConnectionOptions
    ) {
        guard let windowScene = scene as? UIWindowScene else { return }

        configureAudioSession()

        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = ViewController()
        window.makeKeyAndVisible()
        self.window = window
    }

    // MARK: - Audio Session

    private func configureAudioSession() {
        do {
            // .playback category: overrides the silent switch and keeps audio running in the background.
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
            try AVAudioSession.sharedInstance().setActive(true)
        } catch {
            #if DEBUG
            print("AVAudioSession setup failed: \(error)")
            #endif
        }

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(handleAudioInterruption),
            name: AVAudioSession.interruptionNotification,
            object: nil
        )
    }

    @objc private func handleAudioInterruption(_ notification: Notification) {
        guard
            let info = notification.userInfo,
            let typeValue = info[AVAudioSessionInterruptionTypeKey] as? UInt,
            let type = AVAudioSession.InterruptionType(rawValue: typeValue)
        else { return }

        let controllable = window?.rootViewController as? NorcoastAudioControllable

        switch type {
        case .began:
            controllable?.pauseForInterruption()
        case .ended:
            try? AVAudioSession.sharedInstance().setActive(true)
            let options = AVAudioSession.InterruptionOptions(
                rawValue: info[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0
            )
            if options.contains(.shouldResume) {
                // Short delay lets the audio system stabilise before resuming.
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                    controllable?.resumeAfterInterruption()
                }
            }
        @unknown default:
            break
        }
    }
}
