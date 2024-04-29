
def log_distance(rssi: int, env_factor: float = 4.0, ref_path_loss: int = 45) -> float:
	return 10**((-rssi - ref_path_loss) / (10 * env_factor))
