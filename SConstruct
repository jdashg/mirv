files = [
    'mirv.cpp',
    'mirv_entrypoints.cpp',
]

ccflags = [
    '-EHsc',
    '-DVC_EXTRALEAN',
]

ENV = Environment(CCFLAGS=ccflags)

# --

try:
    has_d3d12 = bool(int(ENV['D3D12']))
except KeyError:
    has_d3d12 = (ENV['PLATFORM'] == 'win32')

if has_d3d12:
    files += [
        'mirv_d12.cpp',
    ]

# --

ENV.SharedLibrary('vulkan', files)