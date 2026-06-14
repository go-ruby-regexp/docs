<p align="center"><img src="https://raw.githubusercontent.com/go-onigmo/brand/main/social/go-onigmo.png" alt="go-onigmo/docs" width="720"></p>

# go-onigmo/docs

Versioned documentation for [go-onigmo](https://github.com/go-onigmo),
built with [MkDocs Material](https://squidfunk.github.io/mkdocs-material/) and
versioned with [mike](https://github.com/jimporter/mike). Published to the
`gh-pages` branch and served at <https://go-onigmo.github.io/docs/>.

The organization landing page ([go-onigmo.github.io](https://go-onigmo.github.io))
links here.

## Local preview

```bash
python -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt
mkdocs serve                       # http://localhost:8000 (current sources)
mike serve                         # preview the versioned site
```

## Releasing a new docs version

```bash
mike deploy --push --update-aliases <version> latest
mike set-default --push latest
```
