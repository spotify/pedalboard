# How To Install `pedalboard` 

`pedalboard` is a useful tool if you are an artist, musician, producer, or enthusiast getting started with Python and looking to add effects to audio files. 

## Compatibility

For `pedalboard`, ensure you have [Python 3.6](https://www.python.org/downloads/) or higher. 

## Create a Project Directory
Create a folder that will serve as your project directory for `pedalboard`. 

## Virtual Environments

`pedalboard` is delivered as a Python package. When working with packages, you should create  a [virtual environment](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/). This ensures that a package is only added to your current project, rather than your entire operating system. 

You can set up a virtual environment by navigating to your project directory in the terminal and running the following commands:

```
# Create a virtual environment
python3 -m pip install --user virtualenv 
python3 -m venv .venv

# Activate the virtual environment
source .venv/bin/activate
```

Note: Windows commands are available [here](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/).

If you want to leave your `pedalboard` project, you can deactivate the virtual environment using:

```
deactivate
```

You can activate your virtual environment at any time, without needing to create it again.

## Packages

`pedalboard` is a package that can be installed using Python's package manager, [pip](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/). If you have set up a virtual environment, pip will add the package there. You will need to make sure that:

```
# pip is installed
python3 -m pip install --user --upgrade pip

# pip is updated 
python3 -m pip --version
```

## Installing `pedalboard` 

After setting up a virtual environment and installing pip, install `pedalboard`:
```
pip install pedalboard
```` 

## Accessing Audio Files

The [interactive demo](https://colab.research.google.com/drive/1bHjhJj1aCoOlXKl_lOfG99Xs3qWVrhch#scrollTo=J3MBH6-5yz97) for `pedalboard` uses [librosa](https://librosa.org/). Librosa is a popular package that provides access to audio files, as well as tools for music and audio analysis. 

You can install librosa using pip:
```
pip install librosa
```

If you plan to use your own audio files, save them to the project directory. You can use [`librosa.load`](https://librosa.org/doc/main/generated/librosa.load.html) to load an audio file by its path.

## Creating A Python Program

You'll want an IDE to build, run, and debug your code. [Thonny](https://thonny.org/) is a popular choice for beginners, and there are many others.

Open a new Python file in your IDE and save it to your project directory. 

Note: creating a virtual environment will create a new directory (.venv) within your project directory. Save your .py file in the project directory. 

You can now refer to the [interactive demo](https://colab.research.google.com/drive/1bHjhJj1aCoOlXKl_lOfG99Xs3qWVrhch#scrollTo=J3MBH6-5yz97) to understand how audio and effects are called.

The [interactive demo](https://colab.research.google.com/drive/1bHjhJj1aCoOlXKl_lOfG99Xs3qWVrhch#scrollTo=J3MBH6-5yz97) uses additional packages, including NumPy and Matplotlib. More information is available below.

# Key Terms

- package: a collection of modules (a Python program that you import), bound by a package name. `pedalboard` is a package.
- virtual environment: an environment that allows packages to be installed for a specific project, rather than system wide. 
- [venv](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/): a tool used for creating virtual environments in Python.
- [pip](https://packaging.python.org/guides/installing-using-pip-and-virtual-environments/): a package manager for Python. Allows you to install and use packages, including `pedalboard`. 
- [wheel](https://packaging.python.org/guides/distributing-packages-using-setuptools/#wheels): a way of delivering packages in Python. Allows the user to install the package quicker. `pedalboard` is delivered as a wheel. 
- Integrated Development Environment (IDE): a software application that combines different programming activities, including writing, executing, and debugging code.   

## Other Useful Packages

- [NumPy](https://numpy.org/): a package that facilitates mathematical and other operations on data. NumPy is required by `pedalboard` and will automatically be installed with `pedalboard`.
```
pip install numpy
```

- [Matplotlib](https://matplotlib.org/stable/index.html): a package for data visualization. Matplotlib is referenced in the `pedalboard` [interactive demo](https://matplotlib.org/).
```
pip install matplotlib
```
