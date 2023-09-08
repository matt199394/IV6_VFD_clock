IV6 VFD clock
Homemade IV6 VFD clock with time, date and alarm function.

Code to drive the VFD tube is taken from https://www.valvewizard.co.uk/iv18clock.html and sightly  modified to operate with two ULN2803 instesad of ILQ615.
The structure of the program is a state machine (see swich-case in loop())

From here you can see a breaf demo https://www.youtube.com/watch?v=7VDIsH5IEZ0

In this repo other than the code I have share the gerber file. 
Building the clock is relative simple because of I have used only through hole component hand ready to use module.
 
To set the clock you the buttons as:
  - SW1 to set time then SW2 and SW3 to increase or decase the hours, mins and secs.
  - SW2 to toggle alllarm ON and OFF, if alarma ON pushing SW1 set the alarms hours mins and secs.


Have fun!

Matteo

![Uploading image.png…]()

