rm *.elf *.dol *.gci

make clean
make
dollz3.exe gbiloader.dol boot.dol
dol2gci.exe boot.dol boot.gci