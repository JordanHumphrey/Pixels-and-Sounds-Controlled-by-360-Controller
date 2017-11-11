# Game-Engine
Free time project for making a game engine on winOS

###### Updates are as follows for (11/8/2017)

###### 1) Updated DirectSound functionality, added testing for sound
###### 2) Compartmentalized a few commonly used attributes
###### 3) Changed pixel-offset(x,y) control to LThumb stick on 360 controller
###### 4) Abstraction and clean up will take place very soon, getting additional core components working

######  Currently, an executable does work as intended, and is available here. win32_engine.cpp is a constant work in progress, and is the first file in the entirety of the engine.

###### Download the .exe and plug in a Xbox controller (preferably 360 as was tested for) and move the Left Thumb-Stick.
###### Video file plays sound and shows control of image render via controller thumb stick.

###### -----------------------------------------------------------------------------------------------

###### Updates are as follows for (11/5/2017)

###### 1) Changed XInput get/set state stub(s) to return the corerect error code from Windows, as returning Zero from them actually says it succeeded
###### 2) Updated XInput Library module to try for XInput 1.4 instead of 1.3, since Windows8 does not support 1.3
###### 3) Added support for DirectSound, core function is written, tests will occur in next couple of days.
