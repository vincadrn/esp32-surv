# Low Power Motion- and Proximity-based Surveillance System
In the system, ESP32-CAM is utilized for the main surveillance system. For simple monitoring of the surveillance system, Android app is developed using Flutter.

## ESP32-CAM
External sensors are used for detecting motion and proximity.
For motion detection, SR-602 PIR module sensor is used.
For proximity detection, HC-SR04 ultrasonic module sensor is used.
Interconnection between ESP32-CAM and the external sensors is described as the following.

<img src="https://user-images.githubusercontent.com/42486755/209902583-df9ac7dc-1f0a-4408-a688-c73b37a331df.png" alt="interconnection" style="width:500px">

## Operation of the system
When there is nothing to detect or process, the ESP is set to be in deep sleep mode. PIR sensor is then used to wake the ESP up from the deep sleep to be able to confirm detection using the ultrasonic sensor.
If confirmed, the ESP immediately take a picture and send it to Google Firebase, beside the log and notification using FCM.

## Android app
The app is used for simple monitoring. When a new picture arrives, the user will be notified. The app will fetch the image and show it from the app.

<img src="https://user-images.githubusercontent.com/42486755/209903485-5d55907b-3137-434d-a10a-f2142098a69c.jpg" alt="notification" style="width:200px">

<img src="https://user-images.githubusercontent.com/42486755/209903330-95065090-93f9-4f21-a0b7-8e103344f72c.jpg" alt="app" style="width:200px">

## Endnote and further improvement
This project is done in only a week, so there are many rooms available for improvement. If you want to contribute to that, please do a PR or create an issue.
