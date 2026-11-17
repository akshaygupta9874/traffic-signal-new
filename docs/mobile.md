# Android apps (Guard and Controller)

Two separate apps, **Signal Guard** and **Signal Controller**, are the same web apps from `apps/` wrapped with
Capacitor. They talk to the relay over the local Wi-Fi, so the relay must be running first.

## A. Start the server (on the PC)
Pick one:
* **Docker:** `docker compose up --build`
* **Windows without Docker:** double-click `start.bat` (needs Node.js and DOSBox)
* **Linux/macOS:** `./start.sh`

The relay window prints the PC's address, for example `http://192.168.1.20:8080/guard/`.
The part you need for the apps is `192.168.1.20:8080`. Allow Node.js / Docker through the Windows firewall (private network).

## B. Get the two APKs
**Option 1 - GitHub builds them for you (no Android tools needed)**
1. Create a GitHub repository and push this whole folder to it.
2. Open the repository's **Actions** tab, choose **Build Android apps**, wait about 5 minutes.
3. Open the finished run and download the **signal-apks** artifact: `signal-guard.apk`, `signal-controller.apk`.

**Option 2 - build on your PC**
1. Install Android Studio (it brings the Android SDK and JDK 21) and Node.js 20+.
2. Set `ANDROID_HOME` to the SDK folder (Windows: `C:\Users\<you>\AppData\Local\Android\Sdk`).
3. In the project folder: `node mobile/make-apk.js both`
4. The files appear in `mobile/out/`.

## C. Install and use
1. Copy the APK to the phone (USB, WhatsApp to yourself, Drive). Open it and allow "Install unknown apps" when asked.
   It is a debug-signed build, so Play Protect may show a warning; choose "Install anyway".
2. Phone and PC on the **same Wi-Fi**.
3. First start: enter the PC address (`192.168.1.20:8080`). It is remembered; the **Server** button changes it.
4. Controller app also asks for the PIN (default 1234).
Guard phones should stay on the guard app; the screen stays awake while it is open.

## Notes
* Plain `ws://` on a local network is allowed by the build script (Android blocks it by default).
* Everything else (language, layout, behaviour) is shared with the browser version; fixing `apps/` fixes both.
* Not on Play Store: that needs a signed release build and a developer account.
