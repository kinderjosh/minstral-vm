# Minstral VM

Minstral (short for Minimal Instruction Translator) contains a virtual machine, assembler and disassembler.

## Installation

All you need is ```gcc``` and ```make```, then clone the repository and run the makefile:

```bash
git clone https://github.com/kinderjosh/minstral-vm.git
cd minstral-vm
make
```

## Usage

```
./minstral <command> [options] <input file>
```

### Commands

- ```asm``` - Assemble a machine code file.
- ```dis``` - Disassemble a machine code file.
- ```exe``` - Execute a machine code file.
- ```run``` - Assemble and execute a machine code file.

### Options

- ```-decimal``` - Output decimal machine code.
- ```-linebreak``` - Output linebreaks in machine code.
- ```-o <output file>``` - Specify the output filename.

## Examples

### Assembling

Use the ```asm``` command to convert an assembly file to a machine code file.

> [examples/hi.min](./examples/hi.min):

```asm
; simple program that prints "Hi"

prc 'H' ; print character H
prc 'i' ; print character i
prc '\n' ; print a new line
hlt ; stop execution
```

Assemble it:

```console
$ ./minstral asm examples/hi.min
```

a.out:

```asm
101 1001000 101 1101001 101 1010 1 0
```

### Disassembling

Use the ```dis``` command to convert a machine code file to an assembly file.

(Using the same example from above):

```console
$ ./minstral dis a.out
```

dis.min:

```asm
prc 72
prc 105
prc 10
hlt
```

### Executing

Use the ```exe``` command to execute a machine code file.

(Using the same example from above):

```console
$ ./minstral exe a.out
Hi
```

### Running

You can assemble and execute an assembly file at once with the ```run``` command.

```console
$ ./minstral run examples/hi.min
Hi
```

## Syntax Highlighting

Syntax highlighting for VSCode is available in the [editor](./editor/) directory in the form of a VSIX file.

## Documentation

Wiki in progress.

## License

Minstral is distributed under the [BSD 3-Clause](./LICENSE) license.
