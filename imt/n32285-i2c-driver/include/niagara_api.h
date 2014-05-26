#define NIAGARA_DRV_NAME "N32285"
#define NIAGARA_VERSION_MAJOR 1
#define NIAGARA_VERSION_MINOR 0
#define NIAGARA_VERSION_BUILD 1
#define NIAGARA_VERSION ((NIAGARA_VERSION_MAJOR << 16) | (NIAGARA_VERSION_MINOR << 8) | NIAGARA_VERSION_BUILD)

int NiagaraGetAttribute(const char *attribute, unsigned *value);
int NiagaraSetAttribute(const char *attribute, unsigned value);

int NiagaraGetCardName(char *name, size_t namesize);
