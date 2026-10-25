import sys
from PIL import Image, ImageDraw
import requests
from io import BytesIO

def create_collage(image_paths, output_path="output.jpg"):
    # Constants
    canvas_width, canvas_height = 1772, 1181
    image_size = 515  # Size of each circular image
    circle_diameter = 515
    padding_x = (canvas_width - 3 * image_size) // 4  # Horizontal padding
    padding_y = (canvas_height - 2 * image_size) // 3  # Vertical padding

    # Create a white canvas
    canvas = Image.new("RGB", (canvas_width, canvas_height), "white")

    # Process and arrange images
    for idx, image_path in enumerate(image_paths[:6]):
        # Open the image from a file path or URL
        if image_path.startswith("http"):
            response = requests.get(image_path)
            img = Image.open(BytesIO(response.content))
        else:
            img = Image.open(image_path)

        # Crop the image to maintain aspect ratio and center it
        img_ratio = img.width / img.height
        target_ratio = 1  # Square aspect ratio for the circular crop

        if img_ratio > target_ratio:
            # Wider than target: crop sides
            new_width = int(target_ratio * img.height)
            left = (img.width - new_width) // 2
            right = left + new_width
            img = img.crop((left, 0, right, img.height))
        elif img_ratio < target_ratio:
            # Taller than target: crop top and bottom
            new_height = int(img.width / target_ratio)
            top = (img.height - new_height) // 2
            bottom = top + new_height
            img = img.crop((0, top, img.width, bottom))

        # Resize the image to the target size
        img = img.resize((image_size, image_size), Image.Resampling.LANCZOS)

        # Create a circular mask
        mask = Image.new("L", (circle_diameter, circle_diameter), 0)
        draw = ImageDraw.Draw(mask)
        draw.ellipse((0, 0, circle_diameter, circle_diameter), fill=255)

        # Apply the mask to create a circular image
        circular_img = Image.new("RGBA", (circle_diameter, circle_diameter))
        circular_img.paste(img, (0, 0), mask)

        # Calculate the position on the canvas
        row = idx // 3
        col = idx % 3
        x = padding_x + col * (image_size + padding_x)
        y = padding_y + row * (image_size + padding_y)

        # Paste the circular image onto the canvas
        canvas.paste(circular_img, (x, y), circular_img)

    # Save the output image as a high-quality JPG
    canvas.save(output_path, "JPEG", quality=95)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: prepare_tokens.py output.jpg input1.jpg input2.jpg ...")
        sys.exit(1)

    output_path = sys.argv[1]
    image_paths = sys.argv[2:]

    # Create the collage
    create_collage(image_paths, output_path)
    print(f"Collage saved to {output_path}")