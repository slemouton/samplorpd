# Samplor EXTERNAL OBJECTS in Pd.

## pd external example
example Here are the sources for three simple external objects in Pd.
To compile, type "make pd_linux", "nmake pd_nt", "make pd_irix5" or "make
pd_irix6".

The objects "foo1" and "foo2" are intended as very simple control objects, and
"dspobj" is a tilde object.

## to build and test samplor pd version without flext :
cd /Users/lemouton/Projets/MAXMSPStuff/slm.externs/samplor~puredata/samplor-noflext/
make samplorpd
cp samplorpd~.pd_darwin /Applications/Pd-0.48-1.app/Contents/Resources/extra
open -a Pd-0.48-1.app test-samplorpd~.pd

## Build with Sublime Text
/Users/lemouton/Library/Application\ Support/Sublime\ Text\ 3/Packages/User/pd.sublime-build

## todo
run in a debugger !