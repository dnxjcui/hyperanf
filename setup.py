from setuptools import find_packages, setup

setup(
    name="hyperanf",
    packages=find_packages(include=["hyperanf"]),
    version="0.1.0",
    description="HyperANF implementation",
    author="Nick Cui",
    install_requires=["HLL"],
    setup_requires=["pytest-runner"],
    tests_require=["pytest==6.2.5"],
    test_suite="test_hyperanf"
)

