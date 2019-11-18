# foobar2000-SDK

You can always get an original untouched copy here: https://foobar2000.org/SDK

This copy has the following changes applied.

- All projects updated for Visual Studio 2019 by using the `v142` platform toolset and latest Windows 10 SDK.
- All projects have these additional compiler options set: `/permissive- /std:c++17`
- All projects pre-configured to include the bundled copy of `WTL` so `foo_sample` should compile with no fuss.

Because I use this as a Git submodule for all my components, I've added some other personally required dependencies:

[reupen/columns_ui-sdk](https://github.com/reupen/columns_ui-sdk)

[nlohmann/json](https://github.com/nlohmann/json)

[wtl](https://sourceforge.net/projects/wtl/)

Licenses for each project can be found in their respective folders.
