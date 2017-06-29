rm *.elf *.dol *.gci

make clean
make
dolxz.exe gbiloader.dol boot.dol -cube
dol2gci.exe boot.dol boot.gci