# CIRCLS: Controlled InfraRed for Camera to LED Systems

<img src="/doc/screenshot.png" width="360" height=640>

## Building the client

* Clone CIRCLS

```bash
git clone https://github.com/fritzr/circls
```

* Download and unzip OpenCV for Android

```bash
wget https://github.com/opencv/opencv/releases/download/4.8.0/opencv-4.8.0-android-sdk.zip
unzip opencv-4.8.0-android-sdk.zip
```

* Apply CIRCLS opencv patch to orient the camera correctly

```bash
patch -p1 < circls\doc\opencv.diff
```

* Update circls\CirclsClient\local.properties to inclue your java SDK and opencv paths

```
sdk.dir=C\:\\Users\\login\\AppData\\Local\\Android\\Sdk
opencv.dir=C\:\\Users\\login\\Documents\\src\\OpenCV-android-sdk\\sdk
```

* Use Android Studio to build and upload CirclsClient

## Building the server

* Build the following circuit board using a Diffused RGB Common Anode LED, and IR receiver

![circuit](https://raw.githubusercontent.com/fritzr/circls/master/doc/circuit.png)

* Use Arduino IDE to build and upload LedTest
