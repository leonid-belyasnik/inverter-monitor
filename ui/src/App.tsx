import './App.css'

type IconName = 'grid' | 'battery' | 'solar' | 'home' | 'clock' | 'history'

function Icon({ name }: { name: IconName }) {
  const paths: Record<IconName, string> = {
    grid: 'M12 3v18M3 12h18M5.6 5.6l12.8 12.8m0-12.8L5.6 18.4',
    battery: 'M7 7h10v14H7zM10 4h4v3h-4zM10 11h4m-2-2v4',
    solar: 'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v2m0 16v2M4.93 4.93l1.42 1.42m11.3 11.3 1.42 1.42M2 12h2m16 0h2M4.93 19.07l1.42-1.42m11.3-11.3 1.42-1.42',
    home: 'm3 10 9-7 9 7v10a1 1 0 0 1-1 1h-5v-7h-6v7H4a1 1 0 0 1-1-1z',
    clock: 'M12 22a10 10 0 1 0 0-20 10 10 0 0 0 0 20Zm0-16v6l4 2',
    history: 'M3 12a9 9 0 1 0 2.6-6.4L3 8m0-5v5h5m4-1v5l3 2',
  }

  return (
    <svg
      aria-hidden="true"
      className="icon"
      fill="none"
      stroke="currentColor"
      strokeLinecap="round"
      strokeLinejoin="round"
      strokeWidth="1.7"
      viewBox="0 0 24 24"
    >
      <path d={paths[name]} />
    </svg>
  )
}

const metrics: { title: string; icon: IconName; tone: string; description: string }[] = [
  {
    title: 'Напряжение сети',
    icon: 'grid',
    tone: 'violet',
    description: 'Источник пока не подтверждён',
  },
  {
    title: 'Заряд батареи',
    icon: 'battery',
    tone: 'mint',
    description: 'Состояние аккумулятора',
  },
  {
    title: 'Мощность солнечных панелей',
    icon: 'solar',
    tone: 'amber',
    description: 'Текущая генерация PV',
  },
  {
    title: 'Потребление дома',
    icon: 'home',
    tone: 'blue',
    description: 'Текущая нагрузка',
  },
]

function App() {
  return (
    <div className="app-shell">
      <header className="topbar">
        <a className="brand" href="#overview" aria-label="HomePowerGuard — главная">
          <span className="brand-mark" aria-hidden="true">
            <svg fill="none" viewBox="0 0 36 36">
              <path d="m5 16 13-10 13 10v14H5V16Z" />
              <path d="M20 12 13 21h5l-2 7 8-10h-5l1-6Z" />
            </svg>
          </span>
          <span className="brand-name">HomePowerGuard</span>
        </a>

        <nav className="main-nav" aria-label="Основная навигация">
          <a className="nav-link active" href="#overview">Обзор</a>
          <a className="nav-link" href="#history">История</a>
        </nav>

        <div className="connection-pill" role="status">
          <span className="connection-dot" />
          <span>Нет подключения</span>
        </div>
      </header>

      <main>
        <section className="overview-section" id="overview" aria-labelledby="page-title">
          <div className="page-heading">
            <div>
              <p className="eyebrow">МОНИТОРИНГ ЭНЕРГОСИСТЕМЫ</p>
              <h1 id="page-title">Энергия вашего дома</h1>
              <p className="page-description">
                Основные показатели системы и состояние телеметрии в одном месте.
              </p>
            </div>
            <div className="data-state-card">
              <span className="state-icon"><Icon name="history" /></span>
              <span className="state-copy">
                <span className="state-label">Состояние данных</span>
                <strong>Нет подключения</strong>
              </span>
            </div>
          </div>

          <div className="metrics-grid" aria-label="Показатели энергосистемы">
            {metrics.map((metric) => (
              <article className="metric-card" key={metric.title}>
                <div className="metric-card-top">
                  <span className={`metric-icon ${metric.tone}`}><Icon name={metric.icon} /></span>
                  <span className="unavailable-tag">Недоступно</span>
                </div>
                <h2>{metric.title}</h2>
                <p className="metric-value">Недоступно</p>
                <p className="metric-description">{metric.description}</p>
              </article>
            ))}
          </div>

          <section className="timestamps-card" aria-labelledby="timestamps-title">
            <div className="timestamps-heading">
              <span className="timestamps-icon"><Icon name="clock" /></span>
              <div>
                <h2 id="timestamps-title">Обновление данных</h2>
                <p>Время измерения и получения в браузере — разные отметки.</p>
              </div>
            </div>
            <div className="timestamp-values">
              <div className="timestamp-item">
                <span>Последнее измерение</span>
                <strong>Нет данных</strong>
              </div>
              <div className="timestamp-divider" />
              <div className="timestamp-item">
                <span>Получено браузером</span>
                <strong>Нет данных</strong>
              </div>
            </div>
          </section>
        </section>

        <section className="history-section" id="history" aria-labelledby="history-title">
          <div className="section-heading">
            <div>
              <p className="eyebrow">ДИНАМИКА</p>
              <h2 id="history-title">История измерений</h2>
            </div>
            <span className="history-period">Последние измерения</span>
          </div>
          <div className="history-empty">
            <span className="empty-icon"><Icon name="history" /></span>
            <h3>Пока нет данных для графика</h3>
            <p>История появится здесь, когда поступят первые измерения.</p>
          </div>
        </section>
      </main>

      <footer className="page-footer">
        <span>HomePowerGuard</span>
        <span>Локальный мониторинг энергосистемы</span>
      </footer>
    </div>
  )
}

export default App
