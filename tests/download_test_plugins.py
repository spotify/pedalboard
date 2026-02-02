"""
This script is intended to run on GitHub Actions to download plugin files
that we can't embed in the repository itself, but we still want to use for
testing purposes.
"""

import json
import os
import platform

try:
    from google.auth.exceptions import RefreshError
    from google.cloud import storage
    from google.oauth2 import service_account
    HAS_GCS = True
except ImportError:
    HAS_GCS = False

from tqdm import tqdm


def main():
    if not HAS_GCS:
        print("google-cloud-storage not installed. Skipping plugin download.")
        print("Install with: pip install google-cloud-storage")
        return
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

    for plugin_type in ("effect", "instrument"):
        target_filepath = os.path.join(".", "tests", "plugins", plugin_type, platform.system())
        bucket = client.bucket(GCS_ASSET_BUCKET_NAME)
        prefix = f"test-plugins/{plugin_type}/{platform.system()}"

        # Manually iterate here instead of just calling gsutil on the command line as
        # GSUtil on Windows is not 100% guaranteed to install properly on GitHub Actions.
        print(f"Downloading test {plugin_type} plugin files from Google Cloud Storage...")
        try:
            for blob in tqdm(list(bucket.list_blobs(prefix=prefix))):
                local_path = os.path.join(target_filepath, blob.name.replace(prefix + "/", ""))
                if local_path.endswith("/"):
                    os.makedirs(local_path, exist_ok=True)
                else:
                    os.makedirs(os.path.dirname(local_path), exist_ok=True)
                    blob.download_to_filename(local_path)
        except RefreshError as e:
            raise ValueError(
                "Test plugin download failed, likely due to access token expiration. \n"
                "If you're a Spotify employee and are seeing this error, please "
                "regenerate the GCS_READER_SERVICE_ACCOUNT_KEY environment variable "
                "in the GitHub Actions secrets. The existing service account used for "
                "this can be found by searching for a service account containing the "
                "word 'Pedalboard' in the owning team's Google Cloud console IAM page. "
                "If the service account has been deleted, create a new one and give it "
                "read-only access to objects in the GCS_ASSET_BUCKET_NAME bucket."
            ) from e
    print("Done!")


if __name__ == "__main__":
    main()
