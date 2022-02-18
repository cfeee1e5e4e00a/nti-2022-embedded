import os
from pathlib import Path

def options_get():
    with open('options/key_bindings.txt', 'r') as f:
        setup = []
        lines = f.readlines()
        for line in lines:
            setup.append(line.strip('').split()[-1])
        return [x.strip('\'') for x in setup]

def options_set_default():
    with open('options/key_bindings.txt', 'w') as f:
        f.write("save_path_color = 'data/color_faces/'" + '\n')
        f.write("save_path_gray = 'data/gray_faces/'" + '\n')

def check_dirs_data():
    if not Path('data').is_dir():
        os.mkdir('data')
        os.mkdir('data/gray_faces')
    elif not Path('data/gray_faces').is_dir():
        os.mkdir('data/gray_faces')
    if not Path('trained').is_dir():
        os.mkdir('trained')
    if not Path('options').is_dir():
        os.mkdir('options')
        with open('options/users.txt', 'w'): pass
        with open('options/key_bindings.txt', 'w'): options_set_default()
    if not os.path.isfile('options/users.txt'):
        with open('options/users.txt', 'w'): pass
    if not os.path.isfile('options/key_bindings.txt'):
        with open('options/key_bindings.txt', 'w'): options_set_default()

def preprocessing():
    check_dirs_data()
    return [*options_get()]