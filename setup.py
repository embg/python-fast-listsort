from distutils.core import setup
from distutils.extension import Extension

setup(name='fastlist',
      ext_modules = [Extension('fastlist', sources = ['fastlist.c'])])
