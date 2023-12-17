# python3 -Xfrozen_modules=off backup_plan/grid_estimate_octave.py
from oct2py import Oct2Py
import numpy as np


def initialize_octave():
    oc = Oct2Py()
    oc.source('')  # replace with matlab file path
    return oc


def cleanup_octave(oc):
    oc.exit()


def estimate_grid(oc: Oct2Py, base_voltage: int, I_amperage: list, buyer_index: float, seller_index: float, offer: float, system: list):
    result = oc.grid_estimate(base_voltage, I_amperage,
                              buyer_index, seller_index, offer, system)

    rounded_result = round(result, 4)  # Round to 4 decimal
    # If the rounded result is very close to 0, set it to 0 - can be removed later if we want to be ultra precise!
    if abs(rounded_result) < 0.0001:
        rounded_result = 0

    return rounded_result


def validate(oc, **parameters):
    result = estimate_grid(oc, **parameters)
    return result


if __name__ == '__main__':
    validate()
