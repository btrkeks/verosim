# M5 clean-failure regression audit

- input list: `corpora/known_parse_failures_m5.txt`
- expected records: 25
- emitted records: 25
- missing records: 0
- recovered successful loads: 0
- failures without messages: 0

| path | exception | first error |
|---|---|---|
| `train/openscore-lieder/1_kern_conversions/lc5025982.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc5098709.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc5712146.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6197282.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6218728.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6236154.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6474919.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6569855.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6573515.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6575466.krn` | True | exception: vector::_M_default_append |
| `train/openscore-lieder/1_kern_conversions/lc6974646.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmYL1oFYbvNjSy1YSP7GbxBYTx2As6Gv7nUeJSMqfjU2d9.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmSYzHmvKkcy7LkVnmH9BejKfcJzVkZgNTZCt38cWauoVW.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/Qmdqsq9dVoStuQfHfvayroMqSAbzSykSqfThobSmq7qtsg.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmVoKrgBQcPE7esc31XkGSxWasEiU65HuW2wJwRMqgXCQs.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmfKwojamTzc9eL9SsC4vdYxD1vyVHJuJBbJfMkPPNDvR5.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmThGK4no2yoKL6nvFa2o8HFabMcFLphJVvcoBazsPfs6W.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmXHGMWs1b1zu9pRZ5ZAU2HWTEmrADb3ijAopS1Q4K1ef7.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmURMz6Gn4sAamnR3b7zpjgri2XQg65NKQ9LdYKZGXi1FC.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmUcwYeV2QahFGXQZhyrevagLGgRFgQtpvXJ3r475RkjYj.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmXGpmdbEE6hAHHNdQ9JuvvHAhwNyXZHfgWEDoPM5gRPUN.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmTtsF2HGFVd6onPuiUWhBVrHhd8CpvCnezm29MS6L4gEE.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmWmiqUKZZpS2zF8jKRna8QfnFv2k4eiwwLrzze6wofST6.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmaHDXERTd4C2j6RvvqM5dXUysr2rzxeDhmbaAE4D1Kcdb.krn` | True | exception: vector::_M_default_append |
| `train/pdmx/1_kern_conversions/QmasDRy7rFCGZmvQtNxAW2AgRT8iCveUw1JDjW3gBn9gNm.krn` | True | exception: vector::_M_default_append |
