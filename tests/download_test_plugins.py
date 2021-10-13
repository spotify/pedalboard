"""
This script is intended to run on GitHub Actions to download plugin files
that we can't embed in the repository itself, but we still want to use for
testing purposes.
"""

import os
import json
from tqdm import tqdm
import platform
from google.cloud import storage
from google.oauth2 import service_account


def main():
    GCS_ASSET_BUCKET_NAME = os.environ.get("GCS_ASSET_BUCKET_NAME")
    if not GCS_ASSET_BUCKET_NAME:
        print("Missing GCS_ASSET_BUCKET_NAME environment variable! Not downloading.")
        return

    GCS_READER_SERVICE_ACCOUNT_KEY = os.environ.get("GCS_READER_SERVICE_ACCOUNT_KEY")
    if not GCS_READER_SERVICE_ACCOUNT_KEY:
        print("Missing GCS_READER_SERVICE_ACCOUNT_KEY environment variable! Not downloading.")
        return

    json_acct_info = json.loads(GCS_READER_SERVICE_ACCOUNT_KEY)
    credentials = service_account.Credentials.from_service_account_info(json_acct_info)
    client = storage.Client(credentials=credentials)

    target_filepath = os.path.join(".", "tests", "plugins", platform.system())
    bucket = client.bucket(GCS_ASSET_BUCKET_NAME)
    prefix = f"test-plugins/{platform.system()}"

    # Manually iterate here instead of just calling gsutil on the command line as
    # GSUtil on Windows is not 100% guaranteed to install properly on GitHub Actions.
    print("Downloading test plugin files from Google Cloud Storage...")
    for blob in tqdm(list(bucket.list_blobs(prefix=prefix))):
        local_path = os.path.join(target_filepath, blob.name.replace(prefix + "/", ""))
        if local_path.endswith("/"):
            os.makedirs(local_path, exist_ok=True)
        else:
            os.makedirs(os.path.dirname(local_path), exist_ok=True)
            blob.download_to_filename(local_path)
    print("Done!")


if __name__ == "__main__":
    main()
