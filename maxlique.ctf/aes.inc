#ifdef _DEBUG
  printf("tmp_key:");
  for ( size_t i = 0; i < sizeof(tmp_key); ++i )
    printf(" %2.2X", tmp_key[i]);
  printf("\n");
#endif

  unsigned char key[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, tmp_key, sizeof(tmp_key));
  SHA256_Final(key, &sha256);
  
  /* Init vector */
  unsigned char iv[AES_BLOCK_SIZE];
  memset(iv, 0, AES_BLOCK_SIZE);
  if ( s_encode )
  {
    /* AES-128 bit CBC Encryption */
    unsigned char enc_out[sizeof(aes_input)];
    AES_KEY enc_key;
    AES_set_encrypt_key(key, sizeof(key) * 8, &enc_key);
    AES_cbc_encrypt(aes_input, enc_out, sizeof(aes_input), &enc_key, iv, AES_ENCRYPT);
    for ( size_t i = 0; i < sizeof(enc_out); ++i )
    {
      if ( i )
        printf(",");
      printf("0x%2.2X", enc_out[i]);
    }
  } else {
    unsigned char dec_out[sizeof(aes_input)];
    AES_KEY dec_key;
    /* AES-128 bit CBC Decryption */
    AES_set_decrypt_key(key, sizeof(key) * 8, &dec_key); // Size of key is in bits
    AES_cbc_encrypt(s_dec_out, dec_out, sizeof(aes_input), &dec_key, iv, AES_DECRYPT);
    for ( size_t i = 0; i < sizeof(dec_out); ++i )
    {
      printf("%c", dec_out[i]);
    }
  }
  printf("\n");
