# Control and Visualization Software 
## For Princeton Instruments Pixis and Andor iKon-M Cameras
The software is written for Windows NT, and must be compiled using Microsoft Visual Studio C++ 14.0 or higher (2019 Community Edition).

### Drivers
1. Drivers for PI Pixis are provided in `drivers/pi_pixis`. This driver is 32-bit only, and requires a 32-bit Windows to install. Follow the instructions provided in the directory to install the driver. Do not plug in the camera before installing the driver.
2. Drivers for Andor iKon are provided in `drivers/andor_ikon`. This driver is 64-bit compatible and does not require any special installation method. Simply plug in the camera, and in Device Manager point to this directory and Windows will find and install the drivers automatically.

### Pre-requisites
1. Microsoft Visual C++ 2019 Community Edition
2. DirectX SDK (June 2010) 

### To Build
0. Add the location of `rc.exe` to path in order to compile the icon resources (`C:/Program Files/Windows Kits/10/bin/10.0.19041.0/x86/` or similars).
1. Open `cmd`, and execute `vcvarsall.bat x86` (given MSVC is in your path) to load compiler variables.
2. `cd` to the repository.
3. Execute `build_dx9.bat`, this will build the executable under the `build` directory.

### To Run
1. Copy over all files under `runtime` directory to `build`, or any directory where you put the executable.
2. Connect one of the cameras. If both cameras are connected, PI Pixis will have the priority.
3. Double click the executable file (`hitmis.exe`) to run the software.

### Utility
The software provides easy control for cooler temperature, exposure time, continuous exposure, automatic exposure control, region of interest selection, binning, and capture of images as compressed FITS files. It also provides basic statistical analysis on the captured data, such as cross-section or histogram.

### Screenshots
![Main Control Panel](https://github.com/sunipkmukherjee/hitmis_cams/blob/a558d7867177754c4ce997324bd77169213df72e/screenshots/hitmis_win1.PNG)
![Image Display Panel](https://github.com/sunipkmukherjee/hitmis_cams/blob/a558d7867177754c4ce997324bd77169213df72e/screenshots/hitmis_win2.PNG)
![Pixel Histogram Panel](https://github.com/sunipkmukherjee/hitmis_cams/blob/a558d7867177754c4ce997324bd77169213df72e/screenshots/hitmis_win3.PNG)