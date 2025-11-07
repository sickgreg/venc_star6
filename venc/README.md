![OpenIPC logo][logo]

## Open streamer for FPV on Goke SoC's

### Sigmastar infinity6e support

Build the new Sigmastar binary with:

```
make -C venc venc-star6
```

The resulting `venc-star6` binary configures the infinity6e pipeline
at runtime using dynamically loaded MI SDK libraries. Example usage:

```
./venc-star6 --sensor imx335 --fps 60 --host 192.168.0.2 --port 5004 \
  --bitrate 12000 --gop 120 --iq /etc/iq/iqfile.bin
```

Supported sensors are `imx335` and `imx415` with 30/60/90/120 fps
profiles. By default the tool streams H.264 RTP over UDP; switch to
H.265 with `--codec h265`. The streamer automatically loads the
specified IQ binary just like the Divinus project.

[![Telegram](https://openipc.org/images/telegram_button.svg)][telegram]

[logo]: https://openipc.org/assets/openipc-logo-black.svg
[telegram]: https://openipc.org/our-channels
