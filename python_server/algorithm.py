
import math

DEFAULT_REF_PATH_LOSS = 45
DEFAULT_ENV_FACTOR: float = 4.0

def log_distance(rssi: int, env_factor: float = DEFAULT_ENV_FACTOR, ref_path_loss: int = DEFAULT_REF_PATH_LOSS) -> float:
    return 10**((-rssi - ref_path_loss) / (10 * env_factor))

def objective_function(scanners_pos: list[list[float]], distances: list[float], guess: list[float]):
    """Objective function; unused"""
    sum = 0.0
    for i in range(0, len(scanners_pos)):
        err = 0.0
        diff = (guess[0] - scanners_pos[i][0])**2 \
             + (guess[1] - scanners_pos[i][1])**2 \
             + (guess[2] - scanners_pos[i][2])**2
        err += math.sqrt(diff) - distances[i]
    return sum

def gradient(scanners_pos: list[list[float]], distances: list[float], guess: list[float]):
    """Gradient for multilateration equation"""
    grad = [0.0, 0.0, 0.0]
    for i in range(0, len(scanners_pos)):
        dx = guess[0] - scanners_pos[i][0]
        dy = guess[1] - scanners_pos[i][1]
        dz = guess[2] - scanners_pos[i][2]

        rn = math.sqrt(dx**2 + dy**2 + dz**2)
        lhs = 2 * (rn - distances[i])
        grad[0] += lhs * (dx / rn)
        grad[1] += lhs * (dy / rn)
        grad[2] += lhs * (dz / rn)
    return grad

def minimize(initial_guess: list[float] | None, distances: list[float], scanner_pos: list[list[float]],
            iter_limit: int = 1000, learning_rate: int = 0.1, tolerance: int = 1e-6):
    """Minimization. Could be generalized, but it's not really needed in this project."""
    if initial_guess is None:
        # Center of the scanners
        sum_pos = [0.0, 0.0, 0.0]
        for i in range(0, len(scanner_pos)):
            for j in range(0,3):
                sum_pos[j] += scanner_pos[i][j]
        initial_guess = [sum_pos[0] / len(scanner_pos), sum_pos[1] / len(scanner_pos), sum_pos[2] / len(scanner_pos)]

    current_guess = initial_guess
    for _ in range(0, iter_limit):
        grad = gradient(scanner_pos, distances, initial_guess)
        for i in range(0, 3):
            current_guess[i] -= learning_rate * grad[i]

        norm = math.sqrt(grad[0]**2 + grad[1]**2 + grad[2]**2)
        if norm < tolerance:
            break
    return current_guess


if __name__ == '__main__':
    pos = [
        [0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [1.0, 0.0, 0.0],
        [1.0, 1.0, 0.0]
    ]
    guess = [ 1.23, -10.1, 0.0 ]
    distances = [ 0.707, 0.707, 0.707, 0.707 ] # 0.5, 0.5, 0.0
    x,y,z = minimize(guess, distances, pos)
    print(x,y,z)
