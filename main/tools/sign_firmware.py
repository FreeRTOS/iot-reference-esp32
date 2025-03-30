import boto3
import os
import hashlib
import base64

# Key ID (not ARN)
KMS_KEY_ID = "140bbcb2-5a45-4f21-9f3e-5ffec9739fc7"
AWS_REGION = "us-east-2"
AWS_PROFILE = "tennis@charliesfarm"

BUILD_DIR = os.getenv("BUILD_DIR", ".")  # Default ESP-IDF build directory
if not os.path.exists(BUILD_DIR):
    raise FileNotFoundError(f"Build directory does not exist: {BUILD_DIR}")

FIRMWARE_BIN = os.path.join(BUILD_DIR, "firmware.bin")
OUTPUT_SIGNATURE_PATH = os.path.join(BUILD_DIR, "firmware.sig")
OUTPUT_HASH_PATH = os.path.join(BUILD_DIR, "firmware.hsh")

print(f"Using KMS Key ID: {KMS_KEY_ID}")
print(f"Using AWS Region: {AWS_REGION}")
print(f"Using AWS Profile: {AWS_PROFILE}")

def sign_with_kms(firmware_path):
    """Signs the firmware binary using AWS KMS."""
    if not os.path.exists(firmware_path):
        raise FileNotFoundError(f"Firmware binary not found: {firmware_path}")

    # Read the firmware binary
    with open(firmware_path, "rb") as firmware_file:
        firmware_data = firmware_file.read()

    # Compute the SHA256 hash of the firmware
    firmware_hash = hashlib.sha256(firmware_data).digest()


    # Save the hash to a file
    with open(OUTPUT_HASH_PATH, "wb") as hash_file:
        hash_file.write(firmware_hash)
    print(f"Firmware hash saved to {OUTPUT_HASH_PATH}")

    # Create a session with the specified profile
    session = boto3.Session(profile_name=AWS_PROFILE)
    kms_client = session.client("kms", region_name=AWS_REGION)

    # Use the Key ID for signing
    response = kms_client.sign(
        KeyId=KMS_KEY_ID,
        Message=firmware_hash,  # Pre-computed hash
        MessageType="DIGEST",   # Correct
        SigningAlgorithm="RSASSA_PKCS1_V1_5_SHA_256"
    )

    # Save the signature to a file
    signature = response["Signature"]
    with open(OUTPUT_SIGNATURE_PATH, "wb") as sig_file:
        sig_file.write(signature)

    print(f"\033[31mFirmware HASH: {firmware_hash.hex()}\033[0m")
    print(f"Hash Length: {len(firmware_hash)} bytes")
    hash_base64 = base64.b64encode(firmware_hash).decode("utf-8")
    print(f"\033[32mHash (Base64): {hash_base64}\033[0m")

    print(f"\033[31mSignature: {signature.hex()}\033[0m")
    # Print the signature in Base64 format
    signature_base64 = base64.b64encode(signature).decode("utf-8")
    print(f"\033[32mSignature (Base64): {signature_base64}\033[0m")
    print(f"Signature Length: {len(signature)} bytes")
    print(f"Firmware signed successfully. Signature saved to {OUTPUT_SIGNATURE_PATH}")


if __name__ == "__main__":
    print(f"Current directory: {os.getcwd()}")
    sign_with_kms(FIRMWARE_BIN)
