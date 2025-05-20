from setuptools import find_packages, setup

package_name = 'ds_bridge'

setup(
    name=package_name,
    version='1.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Thomas Roth',
    maintainer_email='thomas.roth@nist.gov',
    description='Forward Dataspeed Drive-by-Wire messages to a TCP/IP Server',
    license='NIST Software License',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'bridge = ds_bridge.bridge_node:main',
        ],
    },
)
