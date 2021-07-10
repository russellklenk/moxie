# moxie
The `moxie` project defines a Python 3 module for building high-performance compute pipelines.

## Initial Setup
Follow the [Instructions](https://docs.anaconda.com/anaconda/install/index.html) for installing anaconda3 (miniconda will work as well). Create a new anaconda virtual environment for the latest Python 3.8 (or any later version) using:
```bash
moxie$ conda create --name moxie-py3 python=3.8 -y
moxie$ conda activate moxie-py3
(moxie-py3) moxie$ python --version
Python 3.8.10
```

If you use [Visual Studio Code](https://code.visualstudio.com/), you'll need the C/C++ extension from Microsoft installed, along with the Pylance and Python extensions - also from Microsoft. The `c_cpp_properties.json` and `launch.json` should already be configured and work for you.

To build the module, you first need the `pip build` module installed:
```bash
(moxie-py3) moxie$ python3 -m pip install --upgrade build
```

and then to actually build the module (with extensions):
```bash
(moxie-py3) moxie$ python3 -m build
```

to build just the extensions:
```bash
(moxie-py3) moxie$ python3 setup.py build
```

## Installing a Development Build
Build and install a development build in your environment with:
```bash
(moxie-py3) moxie$ python3 -m build
(moxie-py3) moxie$ conda deactivate
moxie$ conda activate install-env
(install-env) moxie$ cd path/to/target/project
(install-env) project$ python3 -m pip install --no-index --find-links=path/to/moxie/root/dist moxie
```

With this approach you have to uninstall and reinstall the `moxie` module with each change. Alternatively, you can build once and then symlink the resulting `.so` file(s) like so:
```bash
(moxie-py3) moxie$ python3 -m build
(moxie-py3) moxie$ ln -s path/to/moxie/root/build/lib.linux-x86_64/moxie/_moxie_core.cpython-38m-x86_64-linux-gnu.so moxie/
```

This is sufficient to run the unit tests, and is a one-time step - you can do quick builds to pick up changes to the extensions:
```bash
(moxie-py3) moxie$ python3 setup.py build
```

## Running Unit Tests
This library includes extensive unit tests, which can be run using `pytest`:
```bash
(moxie-py3) moxie$ pip install pytest pytest-cov coverage
(moxie-py3) moxie$ pytest
(moxie-py3) moxie$ pytest --cov-report term --cov=moxie
```
