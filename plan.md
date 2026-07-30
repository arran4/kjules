Plan:
1. In `src/newsessiondialog.cpp`: Update `ProxyFilterDataAccessor::getValue()` to handle `public` (and `ispublic`) which returns `!isPrivate`.
2. In `src/advancedfilterproxymodel.cpp`:
   - Add `{"public", SourceModel::ColPrivate}` to `keyToColumn` in `ModelDataAccessor::getValue()`.
   - Update `ModelDataAccessor::getValue()` to intercept `private`, `public`, `fork`, and `archived` keys and return `"true"`/`"false"` by extracting the value from the raw data (`SourceModel::RawDataRole`), rather than returning the localized `"Yes"`/`"No"`. This will fix filtering for these booleans in `AdvancedFilterProxyModel` and also correctly map `public` to `!isPrivate`.
3. In `src/filtereditor.cpp`: Add `public:` to the palette items in the constructor and `setSimplifiedMode()` so it's visible in the UI dropdown.
4. Run tests and pre-commit checks to verify.
