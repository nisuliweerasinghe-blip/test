import requests
import os

url = "https://snowfield-botanist-tidings.ngrok-free.dev/predict"
image_path = r"D:\New folder\Documents\PROJECT\test.jpg"

if not os.path.isfile(image_path):
    raise FileNotFoundError(f"Image not found: {image_path}")

data = {
    "temp_c": "28.5",
    "humidity_pct": "75",
    "visibility_km": "4.2"
}

headers = {
    "ngrok-skip-browser-warning": "true"
}

with open(image_path, "rb") as image:
    files = {
        "image": ("test.jpg", image, "image/jpeg")
    }

    response = requests.post(
        url,
        data=data,
        files=files,
        headers=headers,
        timeout=30
    )

print("HTTP status:", response.status_code)
print("Response:", response.text)