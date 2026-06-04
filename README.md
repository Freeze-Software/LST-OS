Leatherback Sea TurtleOS is a ported version of the main operating system TurtleOS that will have other features and searve another purpose.

## Everything

- `src/boot.s`
- `src/linker.ld`
- `src/kernel.c`
- `src/console.c`
- `src/vga.c`
- `src/serial.c`
- `src/keyboard.c`
- `grub/grub.cfg`
- `Makefile`

## How to run
run:
```bash
make clean
make all
make run
```

make just the iso:
```bash
make iso
```

