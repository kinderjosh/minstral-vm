# Mono VM

Mono VM is a 64-bit virtual machine, assembler and disassembler.

## Installation

All you need is ```gcc``` and ```make```, then clone the repository and run the makefile:

```bash
git clone https://github.com/kinderjosh/mono-vm.git
cd mono-vm
make
```

## Usage

```
./monovm <command> [options] <input file>
```

### Commands

- ```asm``` - Assembles a basic machine code file.
- ```dis``` - Disassembles a basic machine code file.
- ```exe``` - Executes a basic machine code file.
- ```run``` - Assembles and executes a basic machine code file.

### Options

- ```-decimal``` - Output decimal machine code.
- ```-linebreak``` - Output linebreaks in machine code.
- ```-o <output file>``` - Specify the output filename.

## Examples

### Assembling

Use the ```asm``` command to convert an assembly file to a machine code file.

> [examples/hi.mn](./examples/hi.mn):

```x86asm
; simple program that prints "Hi"

prc 'H' ; print character H
prc 'i' ; print character i
prc '\n' ; print a new line
hlt ; stop execution
```

Assemble it:

```console
$ ./monovm asm examples/hi.mn
```

a.out:

```x86asm
101 1001000 101 1101001 101 1010 1 0
```

### Disassembling

Use the ```dis``` command to convert a machine code file to an assembly file.

(Using the same example from above):

```console
$ ./monovm dis a.out
```

a.dis.mn:

```x86asm
prc 72
prc 105
prc 10
hlt
```

### Executing

Use the ```exe``` command to execute a machine code file.

(Using the same example from above):

```console
$ ./monovm exe a.out
Hi
```

### Running

You can assemble and execute an assembly file at once with the ```run``` command.

```console
$ ./monovm run examples/hi.mn
Hi
```

## Documentation

Wiki coming soon.

## License

Mono VM is distributed under the [BSD 3-Clause](./LICENSE) license.