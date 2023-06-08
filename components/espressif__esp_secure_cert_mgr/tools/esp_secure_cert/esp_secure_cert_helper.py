from typing import Dict
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend
from cryptography.x509 import load_pem_x509_certificate, load_der_x509_certificate

def load_private_key(key_file_path: str, password: str = None) -> Dict[str, str]:
    """
    Load a private key from a file in either PEM or DER format.

    Args:
        key_file_path (str): Path to the private key file.
        password (str): Password to decrypt the private key, if it is encrypted.

    Returns:
        Dict[str, str]: A dictionary with the `"encoding"` and `"bytes"` keys.
        The `"encoding"` key holds a value of type `str` (a member of the `serialization.Encoding`
        enum) and the `"bytes"` key holds a value of type `bytes`.

    Raises:
        FileNotFoundError: If the private key file cannot be found or read.
        ValueError: If the private key file is not in PEM or DER format.

    """
    result = {}

    try:
        with open(key_file_path, "rb") as key_file:
            key = key_file.read()
    except FileNotFoundError:
        raise FileNotFoundError(f"Key file not found: {key_file_path}")

    try:
        # Attempt to load the key as a PEM-encoded private key
        private_key = serialization.load_pem_private_key(key, password=password, backend=default_backend())
        result["encoding"] = serialization.Encoding.PEM.value
        result["bytes"] = private_key.private_bytes(encoding=serialization.Encoding.PEM,
                                                    format=serialization.PrivateFormat.TraditionalOpenSSL,
                                                    encryption_algorithm=serialization.NoEncryption())
        return result
    except ValueError:
        pass

    try:
        private_key = serialization.load_der_private_key(key, password=password, backend=default_backend())
        result["encoding"] = serialization.Encoding.DER.value
        result["bytes"] = private_key.private_bytes(encoding=serialization.Encoding.DER,
                                                    format=serialization.PrivateFormat.TraditionalOpenSSL,
                                                    encryption_algorithm=serialization.NoEncryption())
        return result
    except ValueError:
        raise ValueError("Unsupported key encoding format, Please provide PEM or DER encoded key")


def load_certificate(cert_file_path: str) -> Dict[str, str]:
    """
    Load a certificate from a file in either PEM or DER format.

    Args:
        cert_file_path (str): The path to the certificate file.

    Returns:
        Dict[str, str]: A dictionary with the `"encoding"` and `"bytes"` keys.
        The `"encoding"` key holds a value of type `str` (a member of the `serialization.Encoding`
        enum) and the `"bytes"` key holds a value of type `bytes`.

    Raises:
        FileNotFoundError: If the certificate file cannot be found or read.
        ValueError: If the certificate file is not in PEM or DER format.
    """
    result = {}

    try:
        with open(cert_file_path, "rb") as cert_file:
            cert_data = cert_file.read()
    except FileNotFoundError:
        raise FileNotFoundError(f"Cert file not found: {cert_file_path}")

    try:
        cert = load_pem_x509_certificate(cert_data, backend=default_backend())
        result["encoding"] = serialization.Encoding.PEM.value
        result["bytes"] = cert.public_bytes(encoding=serialization.Encoding.PEM)
        return result
    except ValueError:
        pass

    try:
        cert = load_der_x509_certificate(cert_data, backend=default_backend())
        result["encoding"] = serialization.Encoding.DER.value
        result["bytes"] = cert.public_bytes(encoding=serialization.Encoding.DER)
        return result
    except ValueError:
        raise ValueError("Unsupported certificate encoding format, Please provide PEM or DER encoded certificate")

