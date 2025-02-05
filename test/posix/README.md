
# How to run Github tests locally

1. Install Go:
   ```bash
   brew install go
   ```

2. Clone the repository:
   ```bash
   git clone git@github.com:nektos/act.git
   ```

3. Build and install:
   ```bash
   make install
   ```

4. Create a `.env` file with the following content:
   ```
   TRACKLE_CLIENT_ID_LIB_TEST
   TRACKLE_CLIENT_SECRET_LIB_TEST
   TRACKLE_ID_LIB_TEST
   TRACKLE_PRIVATE_KEY_LIB_TEST
   ```

5. From the `test/posix` folder, run:
   ```bash
   docker build -t github-test .
   ```

6. From the root folder, run:
   ```bash
   act -P self-hosted=github-test -j test_on_push --pull=false --secret-file test/posix/.env
   ```
