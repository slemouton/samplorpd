# Samplor in Pd - Serge Lemouton - 2020
# version without flext

## dependencies
puredata (0.49 built from sources)
libxml2 (for importing machfive presets in samplorkg)

## to build and test samplor external for pd :

### Build with command line
cd /Users/lemouton/Projets/MAXMSPStuff/slm.externs/samplor~puredata/samplor-noflext/

make samplorpd

cp samplorpd~.pd_darwin /Applications/Pd-0.48-1.app/Contents/Resources/extra

open -a Pd-0.48-1.app test-samplorpd~.pd

### for Raspberry-pi

make -f makefile.raspberry samplorpd
sudo apt-get install libxml2-dev
pd test-samplorpd~.pd 
tested with a Kork nanoKeys midikeyboard and alsa driver, you may have to :
aconnect 12:0128:0

### Build with Sublime Text
/Users/lemouton/Library/Application\ Support/Sublime\ Text\ 3/Packages/User/pd.sublime-build

### Xcode
xcode allow debuging

## todo

### hashtable in C

### free method

### pthread_mutex_lock

## thanks to 
- miller puckette
- iem16-externals
- Pd Objects - Xcode project for Pd object developement - Created by Cooper Baker

