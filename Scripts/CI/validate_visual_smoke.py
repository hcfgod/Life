import argparse
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate simple PPM visual smoke outputs.")
    parser.add_argument("--image", action="append", required=True, help="PPM P3 image to validate.")
    parser.add_argument("--min-width", type=int, default=32)
    parser.add_argument("--min-height", type=int, default=32)
    parser.add_argument("--min-unique-colors", type=int, default=2)
    return parser.parse_args()


def read_ppm_p3(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    tokens: list[str] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            tokens.extend(line.split())

    if len(tokens) < 4 or tokens[0] != "P3":
        raise ValueError(f"{path} is not a P3 PPM image.")
    width = int(tokens[1])
    height = int(tokens[2])
    max_value = int(tokens[3])
    if max_value <= 0:
        raise ValueError(f"{path} has invalid max value {max_value}.")

    values = [int(token) for token in tokens[4:]]
    if len(values) < width * height * 3:
        raise ValueError(f"{path} has incomplete pixel data.")

    pixels = []
    for index in range(0, width * height * 3, 3):
        pixels.append((values[index], values[index + 1], values[index + 2]))
    return width, height, pixels


def validate_image(path: Path, min_width: int, min_height: int, min_unique_colors: int) -> None:
    width, height, pixels = read_ppm_p3(path)
    if width < min_width or height < min_height:
        raise ValueError(f"{path} is too small: {width}x{height}.")
    unique_colors = set(pixels)
    if len(unique_colors) < min_unique_colors:
        raise ValueError(f"{path} has only {len(unique_colors)} unique colors.")
    if all(pixel == (0, 0, 0) for pixel in pixels):
        raise ValueError(f"{path} is all black.")
    print(f"[CI] Visual smoke image validated: {path} ({width}x{height}, {len(unique_colors)} colors)")


def main() -> int:
    args = parse_arguments()
    for image in args.image:
        validate_image(Path(image), args.min_width, args.min_height, args.min_unique_colors)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
