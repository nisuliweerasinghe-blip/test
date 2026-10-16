
from datetime import datetime
import os

from flask import Flask, request, jsonify

app = Flask(__name__)

# Configuration
UPLOAD_FOLDER = "uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

app.config["MAX_CONTENT_LENGTH"] = 10 * 1024 * 1024  # 10 MB


def compute_risk(temp_c, humidity, visibility_km, image_bytes=None):
    """Calculate risk from temperature, humidity, and visibility."""

    risk = 0.0

    risk += min(max((temp_c - 25) / 20, 0), 1) * 0.3
    risk += min(max((humidity - 60) / 40, 0), 1) * 0.3
    risk += min(max((10 - visibility_km) / 10, 0), 1) * 0.4

    return round(risk, 3)


@app.route("/", methods=["GET"])
def home():
    return jsonify(
        message="Prediction API is running",
        endpoint="/predict",
        method="POST"
    ), 200


@app.route("/predict", methods=["POST"])
def predict():
    try:
        # 1. Read JSON or form data
        if request.is_json:
            data = request.get_json(silent=True)

            if not isinstance(data, dict):
                return jsonify(
                    error="Invalid JSON. Send a JSON object."
                ), 400
        else:
            data = request.form.to_dict()

        if not data:
            return jsonify(
                error="No data received. Send JSON or form data."
            ), 400

        # 2. Validate required fields
        required_fields = [
            "temp_c",
            "humidity_pct",
            "visibility_km"
        ]

        missing_fields = [
            field for field in required_fields
            if field not in data
        ]

        if missing_fields:
            return jsonify(
                error="Missing required fields",
                missing=missing_fields
            ), 400

        # 3. Convert and validate numeric inputs
        try:
            temp_c = float(data["temp_c"])
            humidity = float(data["humidity_pct"])
            visibility = float(data["visibility_km"])
        except (ValueError, TypeError):
            return jsonify(
                error="Sensor values must be numeric"
            ), 400

        # Reject NaN and infinity
        values = [temp_c, humidity, visibility]

        if not all(map(lambda x: __import__("math").isfinite(x), values)):
            return jsonify(
                error="Sensor values must be finite numbers"
            ), 400

        # 4. Optional image upload
        image = None
        uploaded_file = request.files.get("image")

        if uploaded_file and uploaded_file.filename:
            image = uploaded_file.read()

            if image:
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                filename = f"{timestamp}.jpg"
                filepath = os.path.join(UPLOAD_FOLDER, filename)

                with open(filepath, "wb") as file:
                    file.write(image)

        # 5. Calculate risk and prediction
        risk = compute_risk(
            temp_c,
            humidity,
            visibility,
            image
        )

        score = int((1 - risk) * 100)

        if risk < 0.35:
            decision = "SAFE"
        elif risk < 0.65:
            decision = "CAUTION"
        else:
            decision = "DANGER"

        # 6. Log request results
        print(
            f"Temperature={temp_c}, "
            f"Humidity={humidity}, "
            f"Visibility={visibility}, "
            f"Image={'yes' if image else 'no'}, "
            f"Decision={decision}, "
            f"Score={score}, Risk={risk}"
        )

        # 7. Return JSON response
        return jsonify(
            success=True,
            score=score,
            decision=decision,
            risk=risk
        ), 200

    except Exception as e:
        app.logger.exception("Prediction endpoint failed")

        return jsonify(
            error="Internal server error",
            details=str(e)
        ), 500


@app.errorhandler(413)
def file_too_large(error):
    return jsonify(
        error="Request too large. Maximum size is 10 MB."
    ), 413


if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False
    )