xdg-cluster-receiver
--------------------

This is a variant of the cluster-demo-receiver but without any toolkit
involvement, using wayland-protocols (to gain access to XDG-Shell) and
agl-shell* private extensions provided by the compositor.

We use XDG-Shell to create a top-level XDG window and set an application id for
it. We use agl-shell-desktop to be able to position indepedently the surface
on top of the cluster-dashbboard application, and in the same time specify
a bounding box.

Underneath, waylandsink requires a parent surface (wl_surface) as to create a
sub-subsurface where it will draw, on its own, the incoming stream.

We don't pass out that parent surface to the compositor, but instead of use the
app_id to identify applications, that is why it is import to set, for the parent
surface an application id.
