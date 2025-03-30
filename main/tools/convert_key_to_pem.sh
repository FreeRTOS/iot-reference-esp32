#!/bin/bash

# Get the Base64 public key from AWS KMS
aws kms get-public-key \
    --key-id "$1" \
    --profile tennis@charliesfarm \
    --region us-east-2 \
    --output text \
    --query PublicKey | \
    awk 'BEGIN {print "-----BEGIN PUBLIC KEY-----"} {print $0} END {print "-----END PUBLIC KEY-----"}' | \
    fold -w 64 > public_key.pem

echo "Public key saved to public_key.pem"
