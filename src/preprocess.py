# Import required libraries.
import io
import os
import sys
from PIL import Image
import requests


def main():
    """
    Preprocesses the APOD image from the APOD API. The image is converted from 
    a progressive JPEG to a baseline JPEG, and resized to fit a TFT screen. The 
    original aspect ratio of the image is preserved.
    """

    # Setup the APOD API URL.
    apod_api_key = os.getenv("APOD_API_KEY")
    apod_api_url = f"https://api.nasa.gov/planetary/apod?api_key={apod_api_key}&thumbs=True"

    # Send a GET request to the APOD API URL.
    response = requests.get(apod_api_url)

    # If the GET request was unsuccessful, exit.
    if response.status_code != 200:
        print(f"Error Contacting APOD API")
        print(f"Response Code: {response.status_code}")
        sys.exit(1)

    # Store the APOD API's JSON response.
    apod_api_json = response.json()

    # Create a variable to store the APOD image URL.
    apod_image_url = None

    # If APOD is an image, store the image URL.
    if apod_api_json["media_type"] == "image":
        apod_image_url = apod_api_json["url"]

    # If APOD is a video, store the video thumbnail URL.
    elif apod_api_json["media_type"] == "video":
        apod_image_url = apod_api_json["thumbnail_url"]

    # Otherwise, exit.
    else:
        print(f"Error Parsing APOD API JSON")
        sys.exit(1)

    # Send a GET request to the APOD image URL.
    response = requests.get(apod_image_url)

    # If the GET request was unsuccessful, exit.
    if response.status_code != 200:
        print("Error Downloading APOD Image")
        print(f"Response Code: {response.status_code}")
        sys.exit(1)

    # Load the APOD image.
    image = Image.open(io.BytesIO(response.content))

    # Store the screen width and height.
    screen_width, screen_height = 320, 240

    # Store the image width and height.
    image_width, image_height = image.size

    # Create variables to store the scaled width and height.
    scaled_width, scaled_height = None, None

    # If the image is not square, scale in accordance.
    if image_width != image_height:
        # If the width is larger than the height.
        if image_width > image_height:
            # Scale the width to the screen width.
            scaled_width = screen_width

            # Scale the height while maintaining the aspect ratio.
            scaled_height = int((scaled_width / image_width) * image_height)

        # If the height is larger than the width.
        else:
            # Scale the height to the screen height.
            scaled_height = screen_height

            # Scale the width while maintaining the aspect ratio.
            scaled_width = int((scaled_height / image_height) * image_width)

    # If the image is square, scale in accordance.
    else:
        # Scale both the width and height to the same size.
        scaled_width = scaled_height = min(screen_width, screen_height)

    # Resize the APOD image to fit the screen.
    image = image.resize((scaled_width, scaled_height), resample=Image.Resampling.LANCZOS)

    # Save the APOD image as a baseline JPEG.
    image.save("../image/apod.jpg", "JPEG", progressive=False)


if __name__ == "__main__":
    main()
