import os
import torch
from torch.utils.ffi import create_extension

this_dir = os.path.abspath(os.path.dirname(__file__))

sources = ['mfc/src/noop_wrapper.cc']
headers = ['mfc/src/noop_wrapper.hh']
defines = []
with_cuda = False

if torch.cuda.is_available():
    print('Including CUDA code.')
    sources += ['mfc/src/cuda_functions.cc']
    headers += ['mfc/src/cuda_functions.hh']
    defines += [('WITH_CUDA', None)]
    with_cuda = True

include_dirs = map(lambda x: os.path.join(this_dir, x), ['../src/modules'])
library_dirs = map(lambda x: os.path.join(this_dir, x), ['../src/modules'])
runtime_library_dirs = map(lambda x: os.path.join(this_dir, x), ['../src/modules'])
libraries = ['noop']

extra_compile_args = ['-std=c++14', '-pthread', '-Wall', '-Wextra']

ffi = create_extension(
    'mfc._ext.mfc_wrapper',
    package=True,
    headers=list(headers),
    sources=list(sources),
    include_dirs=list(include_dirs),
    library_dirs=list(library_dirs),
    runtime_library_dirs=list(runtime_library_dirs),
    libraries=list(libraries),
    define_macros=list(defines),
    extra_compile_args=extra_compile_args,
    relative_to=__file__,
    with_cuda=with_cuda
)

if __name__ == '__main__':
    ffi.build()
