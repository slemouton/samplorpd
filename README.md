# Samplor in Pd - Serge Lemouton - 2020
# version without flext

## dependencies
puredata (0.49 built from sources)

## to build and test samplor external for pd :

### command line
cd /Users/lemouton/Projets/MAXMSPStuff/slm.externs/samplor~puredata/samplor-noflext/
make samplorpd
cp samplorpd~.pd_darwin /Applications/Pd-0.48-1.app/Contents/Resources/extra
open -a Pd-0.48-1.app test-samplorpd~.pd

### Build with Sublime Text
/Users/lemouton/Library/Application\ Support/Sublime\ Text\ 3/Packages/User/pd.sublime-build

### Xcode
todo xcode allow debuging

## todo
###array access
cf /Users/lemouton/Projets/PureData/iem16-externals

###hashtable in C

###free method

###pthread_mutex_lock

## thanks to 
- miller puckette
- iem16-externals
- Pd Objects - Xcode project for Pd object developement - Created by Cooper Baker

