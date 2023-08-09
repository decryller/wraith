**Any actions taken based on the provided code and guidance herein are undertaken at the sole discretion and responsibility of the individual who performs them.**
## About Wraith
Wraith is an open source fully external Minecraft Ghost Client for Linux.\
Memory operations powered by [alma](https://github.com/decryller/alma)\
GUI powered by [RVMT](https://github.com/decryller/RVMT)\
RNG powered by [pcg-cpp](https://github.com/imneme/pcg-cpp)

### Features
- Autoclicker
- Rightclicker
- Reach

### Dependencies
- libx11

### Supported versions
- Forge 1.7.10
- Forge 1.8.9
- Lunar 1.7.10
- Lunar 1.8.9

### Downloading Wraith
**Quick Way:**
```
curl -LJ0 https://github.com/decryller/wraith/releases/download/v1.0-stable/wraith.AppImage -o wraith.AppImage
chmod +x wraith.AppImage
```
**Compiling from source:** 
```
git clone https://github.com/decryller/wraith.git
cd wraith
g++ -std=c++17 -lX11 libs/rvmt/rvmt.cpp libs/alma/alma.cpp main.cpp -o wraith.AppImage
```
Make sure to erase these commands from your terminal's history.\
**For bash**: Run `history -c` before closing the terminal / Delete them directly from `~/.bash_history`

### **Running Wraith**
`sudo` will leave an easily retrievable trace accessible via `journalctl`. Don't use it.\
![sudo journalctl detection](https://i.imgur.com/3aYuxLc.png)
What you can do instead is switch to the root user, run Wraith, erase the current session's command history, and leave.
```
su root
./wraith & disown && history -c && exit
```
The session's opening and closing dates will be stored in the journals, but commands ran in the session won't.
![switching user to root](https://i.imgur.com/FeSFzRD.png)
![no traces of wraith](https://i.imgur.com/i7ZFyXs.png)

### Contact
E-mail: decryller@gmail.com\
Discord: decryller
