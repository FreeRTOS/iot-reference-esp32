#!/usr/bin/env python
# SPDX-FileCopyrightText: 2020-2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import argparse
import os
import sys
from esp_secure_cert import nvs_format, custflash_format
from esp_secure_cert import configure_ds, tlv_format

idf_path = os.getenv('IDF_PATH')
if not idf_path or not os.path.exists(idf_path):
    raise Exception('IDF_PATH not found')

# Check python version is proper or not to avoid script failure
assert sys.version_info >= (3, 6, 0), 'Python version too low.'

esp_secure_cert_data_dir = 'esp_secure_cert_data'
# hmac_key_file is generated when HMAC_KEY is calculated,
# it is used when burning HMAC_KEY to efuse
hmac_key_file = os.path.join(esp_secure_cert_data_dir, 'hmac_key.bin')
# csv and bin filenames are default filenames
# for nvs partition files created with this script
csv_filename = os.path.join(esp_secure_cert_data_dir, 'esp_secure_cert.csv')
bin_filename = os.path.join(esp_secure_cert_data_dir, 'esp_secure_cert.bin')
# Targets supported by the script
supported_targets = {'esp32', 'esp32s2', 'esp32c3', 'esp32s3', 'esp32c6', 'esp32h2'}


# Flash esp_secure_cert partition after its generation
#
# @info
# The partition shall be flashed at the offset provided
# for the --sec_cert_part_offset option
def flash_esp_secure_cert_partition(args, idf_target):
    print('Flashing the esp_secure_cert partition at {0} offset'
          .format(args.sec_cert_part_offset))
    print('Note: You can skip this step by providing --skip_flash argument')

    os.system('python {0}/components/esptool_py/esptool/esptool.py '
              '--chip {1} -p {2} write_flash '
              '{3} {4}'
              .format((idf_path), (idf_target), (args.port),
                      args.sec_cert_part_offset, bin_filename))


def cleanup(args):
    if args.keep_ds_data is False:
        if os.path.exists(hmac_key_file):
            os.remove(hmac_key_file)
        if os.path.exists(csv_filename):
            os.remove(csv_filename)


def main():
    parser = argparse.ArgumentParser(description='''
    The python utility helps to configure and provision
    the device with PKI credentials, to generate the esp_secure_cert partition.
    The utility also configures the DS peripheral on the SoC if available.
    ''')

    parser.add_argument(
        '--private-key',
        dest='privkey',
        default='client.key',
        metavar='relative/path/to/client-priv-key',
        help='relative path to client private key')

    parser.add_argument(
        '--pwd', '--password',
        dest='priv_key_pass',
        metavar='[password]',
        help='the password associated with the private key')

    parser.add_argument(
        '--device-cert',
        dest='device_cert',
        default='client.crt',
        metavar='relative/path/to/device-cert',
        help='relative path to device/client certificate '
             '(which contains the public part of the client private key) ')

    parser.add_argument(
        '--ca-cert',
        dest='ca_cert',
        default='ca.crt',
        metavar='relative/path/to/ca-cert',
        help='relative path to ca certificate which '
             'has been used to sign the client certificate')

    parser.add_argument(
        '--target_chip',
        dest='target_chip', type=str,
        choices=supported_targets,
        default='esp32c3',
        metavar='target chip',
        help='The target chip e.g. esp32s2, s3, c3')

    parser.add_argument(
        '--summary',
        dest='summary', action='store_true',
        help='Provide this option to print efuse summary of the chip')

    parser.add_argument(
        '--secure_cert_type',
        dest='sec_cert_type', type=str,
        choices={'cust_flash_tlv', 'cust_flash', 'nvs'},
        default='cust_flash_tlv',
        metavar='type of secure_cert partition',
        help='The type of esp_secure_cert partition. '
             'Can be \"cust_flash_tlv\" or \"cust_flash\" or \"nvs\". '
             'Please note that \"cust_flash\" and \"nvs\" are legacy formats.')

    parser.add_argument(
        '--configure_ds',
        dest='configure_ds', action='store_true',
        help='Provide this option to configure the DS peripheral.')

    parser.add_argument(
        '--skip_flash',
        dest='skip_flash', action='store_true',
        help='Provide this option to skip flashing the'
             ' esp_secure_cert partition at the value'
             ' provided to sec_cert_part_offset option')

    parser.add_argument(
        '--efuse_key_id',
        dest='efuse_key_id', type=int, choices=range(0, 6),
        metavar='[key_id] ',
        default=1,
        help='Provide the efuse key_id which '
             'contains/will contain HMAC_KEY, default is 1')

    parser.add_argument(
        '--port', '-p',
        dest='port',
        metavar='[port]',
        required=True,
        help='UART com port to which the ESP device is connected')

    parser.add_argument(
        '--keep_ds_data_on_host', '-keep_ds_data',
        dest='keep_ds_data', action='store_true',
        help='Keep encrypted private key data and key '
             'on host machine for testing purpose')

    parser.add_argument(
        '--production', '-prod',
        dest='production', action='store_true',
        help='Enable production configurations. '
             'e.g.keep efuse key block read protection enabled')

    parser.add_argument(
        '--sec_cert_part_offset',
        dest='sec_cert_part_offset',
        default='0xD000',
        help='The flash offset of esp_secure_cert partition'
             ' Hex value must be given e.g. 0xD000')

    args = parser.parse_args()

    idf_target = args.target_chip
    if idf_target not in supported_targets:
        if idf_target is not None:
            print('ERROR: The script does not support '
                  'the target {}'.format(idf_target))
        sys.exit(-1)
    idf_target = str(idf_target)

    if args.summary is not False:
        configure_ds.efuse_summary(args, idf_target)
        sys.exit(0)

    if (os.path.exists(args.privkey) is False):
        print('ERROR: The provided private key file does not exist')
        sys.exit(-1)

    if (os.path.exists(args.device_cert) is False):
        print('ERROR: The provided client cert file does not exist')
        sys.exit(-1)

    if (os.path.exists(esp_secure_cert_data_dir) is False):
        os.makedirs(esp_secure_cert_data_dir)

    # Provide CA cert path only if it exists
    ca_cert = None
    if (os.path.exists(args.ca_cert) is True):
        ca_cert = os.path.abspath(args.ca_cert)

    c = None
    iv = None
    key_size = None

    if args.configure_ds is not False:
        # Burn hmac_key on the efuse block (if it is empty) or read it
        # from the efuse block (if the efuse block already contains a key).
        hmac_key = configure_ds.configure_efuse_key_block(args, idf_target,
                                                          hmac_key_file)
        if hmac_key is None:
            sys.exit(-1)

        # Calculate the encrypted private key data along
        # with all other parameters
        c, iv, key_size = configure_ds.calculate_ds_params(args.privkey,
                                                           args.priv_key_pass,
                                                           hmac_key,
                                                           idf_target)
    else:
        print('--configure_ds option not set. '
              'Configuring without use of DS peripheral.')
        print('WARNING: Not Secure.\n'
              'the private shall be stored as plaintext')

    if args.sec_cert_type == 'cust_flash_tlv':
        if args.configure_ds is not False:
            tlv_format.generate_partition_ds(c, iv, args.efuse_key_id,
                                             key_size, args.device_cert,
                                             ca_cert, idf_target,
                                             bin_filename)
        else:
            tlv_format.generate_partition_no_ds(args.device_cert,
                                                ca_cert, args.privkey,
                                                args.priv_key_pass,
                                                idf_target, bin_filename)

    elif args.sec_cert_type == 'cust_flash':
        if args.configure_ds is not False:
            custflash_format.generate_partition_ds(c, iv, args.efuse_key_id,
                                                   key_size, args.device_cert,
                                                   ca_cert, idf_target,
                                                   bin_filename)
        else:
            custflash_format.generate_partition_no_ds(args.device_cert,
                                                      ca_cert,
                                                      args.privkey,
                                                      args.priv_key_pass,
                                                      idf_target, bin_filename)
    elif args.sec_cert_type == 'nvs':
        # Generate csv file for the DS data and generate an NVS partition.
        if args.configure_ds is not False:
            nvs_format.generate_csv_file_ds(c, iv, args.efuse_key_id,
                                            key_size, args.device_cert,
                                            ca_cert, csv_filename)
        else:
            nvs_format.generate_csv_file_no_ds(args.device_cert, ca_cert,
                                               args.privkey,
                                               args.priv_key_pass,
                                               csv_filename)
        nvs_format.generate_partition(csv_filename, bin_filename)

    if args.skip_flash is False:
        flash_esp_secure_cert_partition(args, idf_target)

    cleanup(args)


if __name__ == '__main__':
    main()
