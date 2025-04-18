# Set the APOD API key environment variable.
set -o allexport
source ../.env
set +o allexport

# Run the APOD image preprocessing script.
python3 preprocess.py

# Push the preprocessed APOD image to remote.
git add ../image/apod.jpg
git commit -m chore: update preprocessed APOD image
git push
