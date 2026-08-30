apps_path = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
]

foreach app: apps_path
    install_data(
    join_paths(app, 'mate-notification-properties.png'),
    install_dir: join_paths(mm_iconsdir, 'hicolor', app, 'apps')
  )
endforeach

install_data(
  join_paths('scalable','mate-notification-properties.svg'),
  install_dir: join_paths(mm_iconsdir, 'hicolor', 'scalable', 'apps')
)

