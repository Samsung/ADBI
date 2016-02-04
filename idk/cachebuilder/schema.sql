pragma foreign_keys=ON;

drop table if exists files;
create table files (
    id   integer primary key,
    path text    not null
);

drop table if exists locations;
create table locations (
    id      integer primary key,
    file    integer not null,
    line    integer not null,
    col     integer not null,
    foreign key(file) references files(id)
);

drop index if exists locations_idx;
create index locations_idx on locations(file);

drop table if exists lines;
create table lines (
    addr    integer,
    loc     integer not null,
    foreign key(loc) references locations(id)
);

drop table if exists types;
create table types (
    id          integer primary key,
    kind        integer not null,
    name        text,
    bytes       integer,
    inner       integer,
    loc         integer,

    foreign key (inner) references types(id)
    foreign key (loc)   references locations(id)
);

drop table if exists array_dim;
create table array_dim (
    id          integer not null,
    num         integer not null,
    size        integer,
    foreign key (id) references types(id)
);

drop table if exists members;
create table members (
    id          integer primary key autoincrement,
    parent      integer not null,
    type        integer not null, 
    offset      integer,
    name        text,
    loc         integer,

    foreign key (parent) references types(id),
    foreign key (type) references types(id)
);

drop table if exists enumerators;
create table enumerators (
    id          integer primary key autoincrement,
    parent      integer not null,
    name        text not null,
    value       integer not null, 
    loc         integer,

    foreign key (parent) references types(id)
);

drop table if exists functions;
create table functions (
    id      integer primary key,
    name    text    not null,
    lo      integer,
    hi      integer,
    loc     integer,
    foreign key (loc)   references locations(id)
);

drop table if exists variables;
create table variables (
    id      integer primary key,
    type    integer not null,
    name    text    ,
    global  boolean not null,
    loc     integer,

    foreign key (type)  references types(id),
    foreign key (loc)   references locations(id)
);

drop table if exists params;
create table params (
    func    integer not null,       -- function
    var     integer not null,       -- variable
    idx     integer not null,       -- parameter number

    foreign key (func)  references functions(id),
    foreign key (var)   references variables(id)
);

drop table if exists variables2ranges;
create table variables2ranges (
    var     integer not null,
    lo      integer not null,
    hi      integer not null,

    foreign key (var)   references variables(id)
);

drop table if exists variables2expressions; 
create table variables2expressions (
    var         integer not null,
    lo          integer, 
    hi          integer,
    expr        text not null,

    foreign key (var)   references variables(id)
);

drop table if exists framepointers;     -- function frame pointer information
create table framepointers (
    func     integer not null,
    lo       integer,           -- address range lower bound
    hi       integer,           -- address range higher bound
    expr     text not null,     -- base64 encoded DWARF expression 

    foreign key (func)  references functions(id)
);

drop table if exists cfi;       -- call frame info
create table cfi (
    lo      integer not null,   -- address range lower bound
    hi      integer not null,   -- address range higher bound
    expr    text not null       -- base64 encoded DWARF expression 
);

drop table if exists symbols;
create table symbols (
    id      integer primary key,
    name    text,
    value   integer not null,
    size    integer not null,
    bind    text not null,
    type    text not null,
    vis     text not null,
    shndx   integer not null
);

drop table if exists insnset;
create table insnset (
    addr    integer not null,   -- address range lower bound
    kind    integer not null    -- instruction set kind 
);
