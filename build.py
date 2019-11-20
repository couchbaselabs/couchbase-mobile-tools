# coding=utf-8

try:
    from PyInquirer import print_json, prompt
except ImportError:
    print('PyInquirer is needed for this script, please install it using pip install pyinquirer...')
    raise

try:
    from pyfiglet import Figlet
except ImportError:
    print('PyFiglet is needed for this script, please install it using pip install pyfiglet...')
    raise

import argparse
import shutil
import sys
import os
import subprocess
import multiprocessing
from pathlib import Path
from contextlib import contextmanager

@contextmanager
def cd(newdir):
    prevdir = os.getcwd()
    os.chdir(os.path.expanduser(newdir))
    try:
        yield
    finally:
        os.chdir(prevdir)

def parse_args(args):
    parser = argparse.ArgumentParser(prog="Master Build Script", description = "Build one or more products in the repo")
    parser.add_argument('-o', '--output', metavar='dir', help='The directory to write the output to [default: output/]', default='output/')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    parser.add_argument('-d', '--debug', action='store_true', help='Make a debug build')
    parser.add_argument('--cmake', help='Override the path to cmake, if needed', metavar='path')
    parser.add_argument('--dotnet', help='Override the path to dotnet, if needed', metavar='path')
    parser.add_argument('--build-32-bit', help='Build 32-bit version of the tool, if possible', action='store_true')
    parser.add_argument('programs', metavar="PROG", type=str, nargs='*', help='The programs to build (cblite, cbl-log, LargeDatasetGenerator)')
    return parser.parse_args(args=args)

def print_proper_call(args):
    output = "To call this build non-interactively, next time use build.py "
    quote = False
    for arg in args:
        if quote:
            output += '"' + arg + '" '
            quote = False
        else:
            output += arg + " "
            
        if(arg in ['--cmake', '--output', '--dotnet']):
            quote = True

    width = shutil.get_terminal_size((80, 20)).columns
    print()
    print('*' * width)
    print(output)
    print('*' * width)
    print()

def build_cmake_program(name, args):
    output = args.output if args.output is not None else 'output/'
    os.makedirs(Path(output, name), exist_ok=True)
    config = "-DCMAKE_BUILD_TYPE=MinSizeRel" if not args.debug else "-DCMAKE_BUILD_TYPE=Debug"
    cmake = "cmake" if args.cmake is None else args.cmake
    cmakeDir = str(Path(Path(__file__).resolve().parent, name))
    subprocessArgs = [cmake, config, cmakeDir]
    if not args.build_32_bit:
        subprocessArgs.append("-DCMAKE_GENERATOR_PLATFORM=x64")

    subprocessArgs.append(cmakeDir)
    with cd(Path(output, name)):
        subprocess.run(subprocessArgs)

        if os.name == 'nt':
            result = subprocess.run([cmake, "--build", ".", "--config", "Debug" if args.debug else "MinSizeRel", "--target", name])
            if result.returncode == 0:
                subprocess.run(["explorer", "Debug" if args.debug else "MinSizeRel"])
        else:
            cpu_count = multiprocessing.cpu_count()
            subprocess.run(["make", "-j" + str(cpu_count), name])
            if result.returncode == 0:
                subprocess.run(["open", "."])

def build_dotnet_program(name, args):
    output = args.output if args.output is not None else 'output/'
    os.makedirs(Path(output, name), exist_ok=True)
    dotnet = "dotnet" if args.dotnet is None else args.dotnet
    dotnetDir = str(Path(Path(__file__).resolve().parent, name))
    config = "Release" if not args.debug else "Debug"

    with cd(Path(output, name)):
        subprocessArgs = [dotnet, "build", "-c", config, "-o", ".", dotnetDir]
        result = subprocess.run(subprocessArgs)
        if result.returncode == 0:
            if os.name == 'nt':
                subprocess.run(["explorer", "."])
            else:
                subprocess.run(["open", "."])
    

def build_program(name, args):
    if name == "cbl-log" or name == "cblite":
        build_cmake_program(name, args)
    elif name == "LargeDatasetGenerator":
        build_dotnet_program(name, args)
    

def do_noninteractive_build(args):
    for program in args.programs:
        build_program(program, args)

def do_interactive_build():
    cblite = 'cblite (unsupported)'
    cbllog = 'cbl-log'
    ldg = 'LargeDatasetGenerator (unsupported)'

    cmake = shutil.which('cmake')
    cmakeMessage = 'Enter the path to cmake, if the default is not acceptable:'
    if cmake is None: 
        cmake = ''
        cmakeMessage = 'Enter the path to cmake (unable to locate):'

    dotnet = shutil.which('dotnet')
    dotnetMessage = 'Enter the path to dotnet, if the default is not acceptable:'
    if dotnet is None: 
        dotnet = ''
        dotnetMessage = 'Enter the path to dotnet (unable to locate):'
    
    questions = [
        {
            'name': 'programs',
            'type': 'checkbox',
            'message': 'Which program(s) would you like to build?',
            'choices':[{'name':cbllog, 'checked': True}, {'name': cblite}, {'name': ldg}]
        },
        {
            'name': 'config',
            'type': 'list',
            'message': 'Which configuration?',
            'choices': ['Debug', 'Release']
        },
        {
            'name': 'destination',
            'type': 'input',
            'message': 'Where do you want to save the output?',
            'default': 'output/'
        },
        {
            'name': 'cmake',
            'type': 'input',
            'message': cmakeMessage,
            'default': cmake,
            'when': lambda answers: cbllog in answers['programs'] or cblite in answers['programs']
        },
        {
            'name': 'dotnet',
            'type': 'input',
            'message': dotnetMessage,
            'default': dotnet,
            'when': lambda answers: ldg in answers['programs']
        }
    ]

    answers = prompt(questions)
    reconstructed_args = []
    if answers['config'] == 'Debug':
        reconstructed_args.append("--debug")

    if answers['destination'] != "output/":
        reconstructed_args.append('--output')
        reconstructed_args.append(answers['destination'])

    if 'cmake' in answers and len(answers['cmake']) > 0 and answers['cmake'] != cmake:
        reconstructed_args.append('--cmake')
        reconstructed_args.append(answers['cmake'])

    if 'dotnet' in answers and len(answers['dotnet']) > 0 and answers['dotnet'] != dotnet:
        reconstructed_args.append('--dotnet')
        reconstructed_args.append(answers['dotnet'])

    for program in answers['programs']:
        reconstructed_args.append(program.replace(' (unsupported)', ''))

    do_noninteractive_build(parse_args(reconstructed_args))
    print_proper_call(reconstructed_args)

def main():
    args = parse_args(sys.argv[1:])
    if len(args.programs) == 0:
        print(Figlet().renderText("CB Mobile Tools"))
        do_interactive_build()
    else:
        do_noninteractive_build(args)

if __name__ == "__main__":
    main()