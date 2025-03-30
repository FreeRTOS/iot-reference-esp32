import argparse
import boto3
import hashlib
from cryptography.hazmat.primitives.serialization import load_der_public_key, Encoding, PublicFormat
import subprocess


def fetch_public_key_from_kms(kms_key_id, profile, region):
    """
    Fetch the public key from AWS KMS using the given key ID, AWS profile, and region.
    Convert it from DER to PEM format and save it to a file.
    """
    session = boto3.Session(profile_name=profile, region_name=region)
    client = session.client('kms')
    response = client.get_public_key(KeyId=kms_key_id)

    # The public key is returned in DER format
    public_key_der = response['PublicKey']

    # Convert DER to PEM
    public_key = load_der_public_key(public_key_der)
    public_key_pem = public_key.public_bytes(
        encoding=Encoding.PEM,
        format=PublicFormat.SubjectPublicKeyInfo
    )

    # Save PEM to a file for debugging and external use
    with open("/tmp/public_key.pem", "wb") as f:
        f.write(public_key_pem)

    print(f"Public Key PEM: {public_key.public_bytes(Encoding.PEM, PublicFormat.SubjectPublicKeyInfo).decode()}")

    print("\033[32mPublic key fetched and saved as 'public_key.pem'.\033[0m")
    return public_key


def openssl_verify(public_key_path, signature_path, firmware_path):
    """Use OpenSSL to verify the signature."""
    try:
        # Generate the hash
        hash_cmd = ["openssl", "dgst", "-sha256", "-binary", firmware_path]
        hash_result = subprocess.run(hash_cmd, check=True, capture_output=True)
        firmware_hash = hash_result.stdout

        # Verify the signature
        verify_cmd = [
            "openssl", "pkeyutl", "-verify",
            "-pubin", "-inkey", public_key_path,
            "-sigfile", signature_path,
            "-pkeyopt", "rsa_padding_mode:pkcs1",
            "-pkeyopt", "digest:sha256"
        ]
        verify_result = subprocess.run(verify_cmd, input=firmware_hash, capture_output=True)
        if verify_result.returncode == 0:
            print("\033[32mOpenSSL verification succeeded.\033[0m")
            return True
        else:
            print("\033[31mOpenSSL verification failed:\033[0m", verify_result.stderr.decode())
            return False
    except Exception as e:
        print(f"Error during OpenSSL verification: {e}")
        return False


def verify_firmware_signature(public_key, firmware_path, signature_path):
    """
    Verify the firmware signature using the provided public key.
    """
    # Read firmware binary
    with open(firmware_path, 'rb') as firmware_file:
        firmware_data = firmware_file.read()

    # Compute the firmware's hash
    firmware_hash = hashlib.sha256(firmware_data).digest()
    print(f"\033[34mFirmware HASH: {firmware_hash.hex()}\033[0m")

    # Read signature
    with open(signature_path, 'rb') as sig_file:
        signature = sig_file.read()
        assert len(signature) == 256, f"Unexpected signature length: {len(signature)}"

    print(f"\033[34mFirmware SIGNATURE: {signature.hex()}\033[0m")

    print("\033[34mAttempting OpenSSL verification...\033[0m")
    openssl_verify("/tmp/public_key.pem", signature_path, firmware_path)


def main():
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Verify firmware signature using AWS KMS public key.")
    parser.add_argument("--firmware", required=True, help="Path to the firmware binary file.")
    parser.add_argument("--signature", required=True, help="Path to the firmware signature file.")
    parser.add_argument("--kms-key-id", required=True, help="AWS KMS key ID.")
    parser.add_argument("--aws-profile", required=True, help="AWS CLI profile name.")
    parser.add_argument("--aws-region", required=True, help="AWS region name.")
    args = parser.parse_args()

    # Fetch public key from AWS KMS
    print(f"\033[34mFetching public key from AWS KMS using profile '{args.aws_profile}' and region '{args.aws_region}'...\033[0m")
    public_key = fetch_public_key_from_kms(args.kms_key_id, args.aws_profile, args.aws_region)

    # Verify the firmware's signature
    print("\033[34mVerifying firmware signature...\033[0m")
    verify_firmware_signature(public_key, args.firmware, args.signature)


if __name__ == "__main__":
    main()
