"""Select one explicit OpenCL descriptor while preserving add-on slot indices."""


def select_gpu_slot(devices, token, platform_index=None):
    if not isinstance(token, str) or not token.strip():
        raise ValueError('A nonempty GPU device-name substring is required')
    if platform_index is not None and (type(platform_index) is not int or platform_index < 0):
        raise ValueError('OpenCL platform index must be a nonnegative integer')
    if not isinstance(devices, list) or not devices:
        raise ValueError('No OpenCL GPU descriptors available')
    for device in devices:
        if not isinstance(device, dict) or not isinstance(device.get('device_name'), str):
            raise ValueError('Malformed OpenCL device descriptor')
        if any(type(device.get(key)) is not int or device[key] < 0 for key in ('platform_index', 'device_index')):
            raise ValueError('Malformed OpenCL device indices')
    matches = [index for index, device in enumerate(devices)
               if token.casefold() in device['device_name'].casefold()
               and (platform_index is None or device['platform_index'] == platform_index)]
    if len(matches) != 1:
        raise ValueError(f'Expected one GPU matching {token!r} on platform {platform_index}; found {len(matches)}: {devices}')
    return matches[0]
