1. **Understand the Issue:** The user wants to support `public:true` in addition to `private:false`. They are the same conceptually (a repository is public if it's not private), but `private:false` might be the current way to do it. Currently, `private` seems to mean `isPrivate` in the filtering context, returning "Yes" or "No", or "true" or "false" from `getValue` in accessors.
2. **Locate Filter logic:**
   - In `src/newsessiondialog.cpp` `SourceModelAccessor::getValue` handles `private` and `isprivate`. It checks `isPrivate` in rawData or `private` in github and returns `"true"` or `"false"`.
   - In `src/advancedfilterproxymodel.cpp` `ModelDataAccessor::getValue` translates `private` to `SourceModel::ColPrivate`. Then it gets the string from `data()` which returns "Yes" or "No". Wait, `AdvancedFilterProxyModel` uses `ModelDataAccessor`.
   - `SourceModel::data` returns `"Yes"` or `"No"` for `ColPrivate`.
   - In `src/filterparser.cpp` `KeyValueNode::evaluate(accessor)` does: `val.contains(m_value, Qt::CaseInsensitive);`
   - Wait! `val.contains(m_value)` is checking if the value of the key contains the query string.
   - So if `private:false` works, it's because `m_value` is `"false"`. But `accessor.getValue("private")` returns `"Yes"` or `"No"` for `AdvancedFilterProxyModel` but `"true"` or `"false"` for `NewSessionDialog`'s `SourceModelAccessor`? Let's check `AdvancedFilterProxyModel` again.
   - In `src/advancedfilterproxymodel.cpp` we have `keyToColumn`.
   - The user asked for `public:true` to work exactly like `private:false`.

3. **Check how boolean filters work:**
   If I do `private:false` in the UI, does it work for `AdvancedFilterProxyModel`? It returns `"Yes"` or `"No"`! So `"Yes".contains("false")` is false. `"No".contains("false")` is false. How does `private:false` even work? Ah, wait, `AdvancedFilterProxyModel`'s `ModelDataAccessor` fetches from `SourceModel::data(DisplayRole)`, which returns localized `i18n("Yes")` / `i18n("No")`. Wait, `private:false` would evaluate to `val.contains("false")`. That doesn't match `"Yes"` or `"No"`. Let's run a quick grep for boolean handling in `FilterParser`.

4. **Add `public` as an alias to `private` with reversed boolean values?**
   Let's check `ModelDataAccessor::getValue` in `src/advancedfilterproxymodel.cpp`:
   If we request `public`, we could map it to `SourceModel::ColPrivate`. But wait, its value is `Yes` or `No`! If `m_value` is `"true"`, it's not going to match!
   Let's see how `KeyValueNode::evaluate` works. `val` comes from `getValue`. For `AdvancedFilterProxyModel` it returns `"Yes"` or `"No"`.
   Oh wait, what if `FilterEditor::applyQuickFilter` adds `private:true` or `private:false`?
   Let's check `src/advancedfilterproxymodel.cpp` `ModelDataAccessor::getValue`.
